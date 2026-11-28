#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/legacy_migration_coordinator.hpp"
#include "core/json.hpp"
#include "core/string_utils.hpp"
#include "core/text_file.hpp"
#include "qt_test_support.hpp"
#include "services/library/favorites_service.hpp"
#include "services/library/library_service.hpp"
#include "services/library/playlist_service.hpp"
#include "services/library/recent_service.hpp"
#include "services/settings/config_service.hpp"
#include "services/settings/shortcut_service.hpp"
#include "services/settings/theme_library_service.hpp"
#include "services/settings/ui_state_service.hpp"
#include "ui/controllers/theme_controller.hpp"

using namespace sky;

namespace {

class FakeRemovalSystem final : public ILegacyRemovalSystem {
public:
    std::map<std::string, std::vector<LegacyUninstallEntry>> entries;
    std::vector<std::filesystem::path> processes;
    std::vector<std::pair<std::filesystem::path, std::vector<std::string>>> launches;
    bool launchResult = true;

    std::vector<LegacyUninstallEntry> uninstallEntries(const std::string &appId) const override {
        const auto found = entries.find(appId);
        return found == entries.end() ? std::vector<LegacyUninstallEntry>{} : found->second;
    }

    std::filesystem::path canonicalPath(const std::filesystem::path &path) const override {
        std::error_code ec;
        const auto resolved = std::filesystem::canonical(path, ec);
        return ec ? std::filesystem::path{} : resolved;
    }

    std::vector<std::filesystem::path> runningProcessExecutables() const override {
        return processes;
    }

    bool launch(const std::filesystem::path &exe, const std::vector<std::string> &args) override {
        launches.emplace_back(exe, args);
        return launchResult;
    }
};

void writeFile(const std::filesystem::path &file, const std::string &text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

std::string quoted(const std::filesystem::path &path) {
    return "\"" + fromWide(path.wstring()) + "\"";
}

struct CoordinatorFixture {
    bool appReady = (ensureCoreApp(), true);
    TempDir temp;
    std::filesystem::path legacyRoot = temp.path() / "old";
    std::filesystem::path ownApp = temp.path() / "cpp";
    std::filesystem::path ownData = temp.path() / "cpp" / "data";
    std::filesystem::path ownConfig = temp.path() / "cpp" / "config";
    std::filesystem::path localStorage = temp.path() / "localStorage";
    std::shared_ptr<FakeRemovalSystem> system = std::make_shared<FakeRemovalSystem>();
    bool libraryIntact = true;

    ConfigService config{ownConfig};
    UiStateService uiState{ownData};
    FavoritesService favorites{uiState};
    RecentService recent{uiState};
    PlaylistService playlists{ownData};
    LibraryService library{ownData, playlists};
    ShortcutService shortcuts{config};
    ThemeLibraryService themeLibrary{ownData};
    ThemeController themes{uiState, config, themeLibrary};
    std::unique_ptr<LegacyMigrationCoordinator> coordinator;

    CoordinatorFixture() {
        writeFile(legacyRoot / "resources" / "app" / "package.json",
                  R"({"name":"SkyAutoPiano","version":"1.3.13","main":"src/main/app.js"})");
        writeFile(
            legacyRoot / "resources" / "app" / "data" / "listSheet.json",
            R"([{"name":"Rubia","author":"Studio","transcribedBy":"A","bpm":68,"keyMap":"rubia.json"}])");
        writeFile(legacyRoot / "resources" / "app" / "data" / "rubia.json",
                  R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
        writeFile(legacyRoot / "SkyAutoPiano.exe", "electron");
        writeFile(legacyRoot / "unins000.exe", "inno");
        std::filesystem::create_directories(localStorage);

        config.load();
        uiState.load();
        playlists.load();
        library.load();
        themeLibrary.load();
        rebuildCoordinator();
    }

    void registerLegacyEntry() {
        LegacyUninstallEntry entry;
        entry.source = "HKCU";
        entry.uninstallString = quoted(legacyRoot / "unins000.exe");
        entry.installLocation = fromWide(legacyRoot.wstring());
        entry.displayName = "Sky Auto Piano";
        system->entries[kLegacyElectronAppId] = {entry};
    }

    void rebuildCoordinator() {
        LegacyCoordinatorEnvironment env;
        env.appDirectory = ownApp;
        env.dataDirectory = ownData;
        env.configDirectory = ownConfig;
        env.localStorageDirectory = localStorage;
        env.searchRoots = {legacyRoot};
        env.useRegistryHints = false;
        env.removalSystem = system;
        env.libraryIntactCheck = [this] {
            return libraryIntact;
        };
        coordinator = std::make_unique<LegacyMigrationCoordinator>(library, playlists, favorites,
                                                                   recent, config, shortcuts,
                                                                   themes, uiState, std::move(env));
    }
};

}  // namespace

TEST_CASE("coordinator retries after partial missing-payload import and unlocks verified removal") {
    CoordinatorFixture f;
    f.registerLegacyEntry();

    // First import has a missing payload ("missing.json").
    writeFile(
        f.legacyRoot / "resources" / "app" / "data" / "listSheet.json",
        R"([{"name":"Rubia","author":"Studio","transcribedBy":"A","bpm":68,"keyMap":"rubia.json"},)"
        R"({"name":"Ghost","author":"Studio","transcribedBy":"A","bpm":68,"keyMap":"missing.json"}])");

    bool quickStartUpdated = false;
    LegacyQuickStartState qsState = LegacyQuickStartState::Searching;
    QObject::connect(f.coordinator.get(), &LegacyMigrationCoordinator::quickStartStateChanged,
                     [&](LegacyQuickStartState s, int, int, const QString &) {
                         qsState = s;
                         quickStartUpdated = true;
                     });
    f.coordinator->detect(false, true);
    REQUIRE(waitUntil([&] { return quickStartUpdated; }));
    REQUIRE(qsState == LegacyQuickStartState::Found);
    REQUIRE(f.coordinator->pendingQuickStartPlan().has_value());

    bool imported = false;
    LegacyImportResult firstResult;
    QObject::connect(f.coordinator.get(), &LegacyMigrationCoordinator::importCompleted,
                     [&](const LegacyImportResult &r, bool, const QString &) {
                         firstResult = r;
                         imported = true;
                     });
    REQUIRE(f.coordinator->importPendingQuickStartPlan());
    REQUIRE(waitUntil([&] { return imported; }));
    CHECK(firstResult.verificationStatus == "needs_review");
    CHECK(firstResult.sheets.copied == 1);
    CHECK(firstResult.sheets.failed == 1);
    CHECK_FALSE(f.coordinator->checkRemovalGate().allowed);

    // Restore missing payload and retry import.
    writeFile(f.legacyRoot / "resources" / "app" / "data" / "missing.json",
              R"({"version":2,"keyMap":{"0":["u"]},"songNotes":[]})");
    quickStartUpdated = false;
    f.coordinator->detect(false, true);
    REQUIRE(waitUntil([&] { return quickStartUpdated; }));
    REQUIRE(qsState == LegacyQuickStartState::Found);

    imported = false;
    LegacyImportResult secondResult;
    QObject::connect(f.coordinator.get(), &LegacyMigrationCoordinator::importCompleted,
                     [&](const LegacyImportResult &r, bool, const QString &) {
                         secondResult = r;
                         imported = true;
                     });
    REQUIRE(f.coordinator->importPendingQuickStartPlan());
    REQUIRE(waitUntil([&] { return imported; }));
    CHECK(secondResult.verificationStatus == "verified");
    CHECK(f.library.size() == 2);

    // Simulate restart + fresh verification before removal.
    f.rebuildCoordinator();
    f.coordinator->setCleanupVisibleProvider([] { return true; });
    bool verifiedDone = false;
    bool verifiedOk = false;
    QObject::connect(f.coordinator.get(), &LegacyMigrationCoordinator::verificationFinished,
                     [&](bool ok, int) {
                         verifiedOk = ok;
                         verifiedDone = true;
                     });
    f.coordinator->refreshCleanup();
    REQUIRE(waitUntil([&] { return verifiedDone; }));
    CHECK(verifiedOk);
    CHECK(f.coordinator->checkRemovalGate().allowed);
}

TEST_CASE("coordinator surfaces same-name different-payload conflicts and blocks removal") {
    CoordinatorFixture f;
    f.registerLegacyEntry();

    // Seed native library with same (name, author, transcribedBy) but different payload bytes.
    const auto localIn = f.temp.path() / "in" / "Rubia.json";
    writeFile(
        localIn,
        R"([{"name":"Rubia","author":"Studio","transcribedBy":"A","bpm":68,"songNotes":[{"time":0,"key":"Key1"}]}])");
    bool localImported = false;
    QObject::connect(&f.library, &LibraryService::importFinished,
                     [&](const ImportSummary &) { localImported = true; });
    f.library.importFiles({localIn});
    REQUIRE(waitUntil([&] { return localImported; }));

    auto install = inspectLegacyFolder(f.legacyRoot);
    REQUIRE(install.has_value());
    std::map<std::string, std::string> existingPayloads;
    existingPayloads.emplace(legacySongKey(f.library.entry(0)->json()),
                             f.library.entry(0)->payloadFile());
    auto plan = planLegacyImport(*install, f.library.legacySongKeys(), f.localStorage, f.ownData,
                                 existingPayloads);
    REQUIRE(plan.ok());
    REQUIRE(plan.value().conflicts.size() == 1);

    bool imported = false;
    LegacyImportResult result;
    QObject::connect(f.coordinator.get(), &LegacyMigrationCoordinator::importCompleted,
                     [&](const LegacyImportResult &r, bool, const QString &) {
                         result = r;
                         imported = true;
                     });
    f.coordinator->importPlan(plan.value(), true);
    REQUIRE(waitUntil([&] { return imported; }));
    CHECK(result.verificationStatus == "needs_review");
    CHECK(result.sheets.conflicts == 1);
    const RemovalGate gate = f.coordinator->checkRemovalGate();
    CHECK_FALSE(gate.allowed);
}

TEST_CASE(
    "coordinator blocks removal if legacy app starts running between gate check and execution") {
    CoordinatorFixture f;
    f.registerLegacyEntry();

    auto install = inspectLegacyFolder(f.legacyRoot);
    REQUIRE(install.has_value());
    auto plan = planLegacyImport(*install, {}, f.localStorage, f.ownData, {});
    REQUIRE(plan.ok());

    bool imported = false;
    QObject::connect(f.coordinator.get(), &LegacyMigrationCoordinator::importCompleted,
                     [&](const LegacyImportResult &, bool, const QString &) { imported = true; });
    f.coordinator->importPlan(plan.value(), false);
    REQUIRE(waitUntil([&] { return imported; }));

    f.coordinator->setCleanupVisibleProvider([] { return true; });
    bool verifiedDone = false;
    QObject::connect(f.coordinator.get(), &LegacyMigrationCoordinator::verificationFinished,
                     [&](bool, int) { verifiedDone = true; });
    f.coordinator->refreshCleanup();
    REQUIRE(waitUntil([&] { return verifiedDone; }));
    REQUIRE(f.coordinator->checkRemovalGate().allowed);

    // Legacy app launches right before user confirms removal.
    f.system->processes = {f.legacyRoot / "SkyAutoPiano.exe"};
    RemovalOutcome observed;
    bool outcomeReady = false;
    QObject::connect(f.coordinator.get(), &LegacyMigrationCoordinator::removalOutcomeObserved,
                     [&](const RemovalOutcome &o) {
                         observed = o;
                         outcomeReady = true;
                     });
    f.coordinator->requestRemoval();
    REQUIRE(outcomeReady);
    CHECK(observed.status == RemovalStatus::Blocked);
    CHECK(f.system->launches.empty());
}

TEST_CASE(
    "failed state persistence forces needs_review and shutdown cancels pending worker callbacks") {
    CoordinatorFixture f;
    auto install = inspectLegacyFolder(f.legacyRoot);
    REQUIRE(install.has_value());
    auto plan = planLegacyImport(*install, {}, f.localStorage, f.ownData, {});
    REQUIRE(plan.ok());

    LegacyImportSummary summary;
    summary.imported = 1;
    summary.payloadMap["rubia.json"] = "rubia.json";
    writeFile(f.ownData / "rubia.json", R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");

    const LegacyImportResult failedPersist =
        aggregateLegacyImportResult(plan.value(), summary, LegacyExtrasOutcome{},
                                    legacySourceFingerprint(*install), f.ownData, false);
    CHECK(failedPersist.verificationStatus == "needs_review");

    // Verify shutdown while background detection is queued does not invoke callbacks on destroyed
    // coordinator.
    bool callbackFiredAfterShutdown = false;
    QObject::connect(f.coordinator.get(), &LegacyMigrationCoordinator::quickStartStateChanged,
                     [&](LegacyQuickStartState, int, int, const QString &) {
                         callbackFiredAfterShutdown = true;
                     });
    f.coordinator->detect(false, true);
    f.coordinator->shutdown();
    f.coordinator.reset();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    CHECK_FALSE(callbackFiredAfterShutdown);
}
