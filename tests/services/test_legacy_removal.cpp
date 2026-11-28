#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <utility>

#include "core/string_utils.hpp"
#include "qt_test_support.hpp"
#include "services/library/legacy_removal.hpp"
#include "services/settings/ui_state_service.hpp"

using namespace sky;

namespace {

// Fake system boundary: registry content, process list, canonicalisation
// and launches are all scripted, so no test touches a real registry, a real
// process, or a real uninstaller.
class FakeRemovalSystem final : public ILegacyRemovalSystem {
public:
    std::map<std::string, std::vector<LegacyUninstallEntry>> entries;
    std::vector<std::filesystem::path> processes;
    std::vector<std::pair<std::filesystem::path, std::vector<std::string>>> launches;
    bool launchResult = true;
    std::function<std::filesystem::path(const std::filesystem::path &)> canonicalHook;

    std::vector<LegacyUninstallEntry> uninstallEntries(const std::string &appId) const override
    {
        const auto found = entries.find(appId);
        return found == entries.end() ? std::vector<LegacyUninstallEntry>{} : found->second;
    }

    std::filesystem::path canonicalPath(const std::filesystem::path &path) const override
    {
        if (canonicalHook) {
            return canonicalHook(path);
        }
        std::error_code error;
        const auto resolved = std::filesystem::canonical(path, error);
        return error ? std::filesystem::path{} : resolved;
    }

    std::vector<std::filesystem::path> runningProcessExecutables() const override { return processes; }

    bool launch(const std::filesystem::path &exe, const std::vector<std::string> &args) override
    {
        launches.emplace_back(exe, args);
        return launchResult;
    }
};

void writeFile(const std::filesystem::path &file, const std::string &text)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

std::string quoted(const std::filesystem::path &path)
{
    return "\"" + fromWide(path.wstring()) + "\"";
}

struct RemovalFixture {
    TempDir temp;
    std::filesystem::path legacyRoot = temp.path() / "old";
    std::filesystem::path ownApp = temp.path() / "cpp";
    std::filesystem::path ownData = temp.path() / "cpp" / "data";
    std::shared_ptr<FakeRemovalSystem> system = std::make_shared<FakeRemovalSystem>();
    bool libraryIntact = true;

    RemovalFixture()
    {
        // Plausible Electron layout: resources\app\package.json + data,
        // the app executable, and the Inno uninstaller, all in one tree.
        writeFile(legacyRoot / "resources" / "app" / "package.json",
                  R"({"name": "SkyAutoPiano", "version": "1.3.13", "main": "src/main/app.js"})");
        writeFile(legacyRoot / "resources" / "app" / "data" / "listSheet.json",
                  R"([{"name": "Rubia", "author": "Studio", "transcribedBy": "A", "bpm": 68, "keyMap": "rubia.json"}])");
        writeFile(legacyRoot / "resources" / "app" / "data" / "rubia.json",
                  R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
        writeFile(legacyRoot / "SkyAutoPiano.exe", "electron");
        writeFile(legacyRoot / "unins000.exe", "inno");
        std::filesystem::create_directories(ownData);
        writeFile(ownData / "listSheet.json", "[]");
    }

    std::filesystem::path uninstaller() const { return legacyRoot / "unins000.exe"; }
    std::filesystem::path executable() const { return legacyRoot / "SkyAutoPiano.exe"; }

    LegacyUninstallEntry entry() const
    {
        LegacyUninstallEntry entry;
        entry.source = "HKCU";
        entry.uninstallString = quoted(uninstaller());
        entry.installLocation = fromWide(legacyRoot.wstring());
        entry.displayName = "Sky Auto Piano";
        return entry;
    }

    void registerEntry()
    {
        system->entries[kLegacyElectronAppId] = {entry()};
    }

    LegacyRemovalService service()
    {
        return LegacyRemovalService(system, ownApp, ownData, [this] { return libraryIntact; });
    }

    LegacyImportEvidence verifiedEvidence(const LegacyInstallInfo &info) const
    {
        LegacyImportEvidence evidence;
        evidence.migrationStatus = "verified";
        evidence.sourceFingerprint = info.sourceFingerprint;
        evidence.reVerified = true;
        return evidence;
    }

    LegacyDetection detected()
    {
        registerEntry();
        auto detection = service().detectLegacyInstall();
        REQUIRE(detection.status == LegacyDetectionStatus::Found);
        REQUIRE(detection.install.has_value());
        return detection;
    }
};

bool hasBlocker(const RemovalGate &gate, RemovalBlocker blocker)
{
    return std::find(gate.blockers.begin(), gate.blockers.end(), blocker) != gate.blockers.end();
}

}  // namespace

TEST_CASE("the exact legacy AppId entry is detected and validated")
{
    RemovalFixture fixture;
    auto detection = fixture.detected();
    const LegacyInstallInfo &info = *detection.install;
    CHECK(std::filesystem::equivalent(info.installRoot, fixture.legacyRoot));
    CHECK(std::filesystem::equivalent(info.uninstaller, fixture.uninstaller()));
    CHECK(std::filesystem::equivalent(info.executable, fixture.executable()));
    CHECK(info.legacy.version == "1.3.13");
    CHECK(info.sourceFingerprint == legacySourceFingerprint(info.legacy));
    CHECK(info.registrySource == "HKCU");
    CHECK_FALSE(detection.manualGuidance.empty());
}

TEST_CASE("no legacy AppId entry means not found, with manual guidance")
{
    RemovalFixture fixture;
    const auto detection = fixture.service().detectLegacyInstall();
    CHECK(detection.status == LegacyDetectionStatus::NotFound);
    CHECK_FALSE(detection.install.has_value());
    CHECK_FALSE(detection.manualGuidance.empty());
}

TEST_CASE("the C++ AppId entry and a duplicate display name are never selected")
{
    RemovalFixture fixture;
    // The C++ app's own uninstall entry, sharing the display name and the
    // executable basename. Detection only ever queries the legacy AppId.
    LegacyUninstallEntry cppEntry;
    cppEntry.source = "HKCU";
    cppEntry.uninstallString = quoted(fixture.ownApp / "unins000.exe");
    cppEntry.installLocation = fromWide(fixture.ownApp.wstring());
    cppEntry.displayName = "Sky Auto Piano";
    writeFile(fixture.ownApp / "unins000.exe", "inno");
    fixture.system->entries[kCppAppId] = {cppEntry};

    auto service = fixture.service();
    CHECK(service.detectLegacyInstall().status == LegacyDetectionStatus::NotFound);

    fixture.registerEntry();
    const auto detection = service.detectLegacyInstall();
    REQUIRE(detection.status == LegacyDetectionStatus::Found);
    CHECK(std::filesystem::equivalent(detection.install->installRoot, fixture.legacyRoot));
}

TEST_CASE("a stale UninstallString whose uninstaller is missing is invalid")
{
    RemovalFixture fixture;
    fixture.registerEntry();
    std::filesystem::remove(fixture.uninstaller());
    const auto detection = fixture.service().detectLegacyInstall();
    CHECK(detection.status == LegacyDetectionStatus::Invalid);
    CHECK_FALSE(detection.reason.empty());
}

TEST_CASE("an uninstaller outside the legacy tree is invalid")
{
    RemovalFixture fixture;
    const auto elsewhere = fixture.temp.path() / "elsewhere";
    writeFile(elsewhere / "unins000.exe", "inno");
    LegacyUninstallEntry entry = fixture.entry();
    entry.uninstallString = quoted(elsewhere / "unins000.exe");
    fixture.system->entries[kLegacyElectronAppId] = {entry};
    CHECK(fixture.service().detectLegacyInstall().status == LegacyDetectionStatus::Invalid);
}

TEST_CASE("a non-Inno executable is invalid")
{
    RemovalFixture fixture;
    writeFile(fixture.legacyRoot / "setup.exe", "not-inno");
    LegacyUninstallEntry entry = fixture.entry();
    entry.uninstallString = quoted(fixture.legacyRoot / "setup.exe");
    fixture.system->entries[kLegacyElectronAppId] = {entry};
    CHECK(fixture.service().detectLegacyInstall().status == LegacyDetectionStatus::Invalid);
}

TEST_CASE("malformed quoting in the UninstallString is invalid")
{
    RemovalFixture fixture;
    LegacyUninstallEntry entry = fixture.entry();
    entry.uninstallString = "\"" + fromWide(fixture.uninstaller().wstring());  // unterminated quote
    fixture.system->entries[kLegacyElectronAppId] = {entry};
    CHECK(fixture.service().detectLegacyInstall().status == LegacyDetectionStatus::Invalid);
    CHECK_FALSE(parseUninstallCommand(entry.uninstallString).has_value());
}

TEST_CASE("uninstall commands are parsed without a shell")
{
    const auto quotedArgs = parseUninstallCommand(R"("C:\Old Apps\Sky Auto Piano\unins000.exe" /SILENT /LOG)");
    REQUIRE(quotedArgs.has_value());
    CHECK(quotedArgs->first == std::filesystem::path(L"C:\\Old Apps\\Sky Auto Piano\\unins000.exe"));
    CHECK(quotedArgs->second == std::vector<std::string>{"/SILENT", "/LOG"});

    const auto plain = parseUninstallCommand(R"(C:\SkyAutoPiano\unins000.exe)");
    REQUIRE(plain.has_value());
    CHECK(plain->second.empty());

    CHECK_FALSE(parseUninstallCommand("").has_value());
    CHECK_FALSE(parseUninstallCommand("   ").has_value());
}

TEST_CASE("two different registered roots for the legacy AppId are ambiguous")
{
    RemovalFixture fixture;
    fixture.registerEntry();
    const auto otherRoot = fixture.temp.path() / "other-old";
    writeFile(otherRoot / "unins000.exe", "inno");
    LegacyUninstallEntry second = fixture.entry();
    second.source = "HKLM";
    second.uninstallString = quoted(otherRoot / "unins000.exe");
    second.installLocation = fromWide(otherRoot.wstring());
    fixture.system->entries[kLegacyElectronAppId].push_back(second);
    CHECK(fixture.service().detectLegacyInstall().status == LegacyDetectionStatus::Ambiguous);
}

TEST_CASE("a junction that resolves the legacy root into the C++ tree blocks detection")
{
    RemovalFixture fixture;
    fixture.registerEntry();
    const auto ownApp = fixture.ownApp;
    const auto legacyRoot = fixture.legacyRoot;
    fixture.system->canonicalHook = [ownApp, legacyRoot](const std::filesystem::path &path) {
        std::error_code error;
        if (path == legacyRoot) {
            const auto resolved = std::filesystem::canonical(ownApp, error);
            return error ? ownApp : resolved;
        }
        if (path.parent_path() == legacyRoot) {
            return ownApp / path.filename();
        }
        const auto resolved = std::filesystem::canonical(path, error);
        return error ? std::filesystem::path{} : resolved;
    };
    // The uninstaller now resolves inside the C++ tree: never proceed.
    CHECK(fixture.service().detectLegacyInstall().status == LegacyDetectionStatus::Invalid);
}

TEST_CASE("removal is offered only for a verified, source-matched import")
{
    RemovalFixture fixture;
    auto detection = fixture.detected();
    auto service = fixture.service();
    const auto gate = service.canOfferRemoval(fixture.verifiedEvidence(*detection.install), detection);
    CHECK(gate.allowed);
    CHECK(gate.blockers.empty());
}

TEST_CASE("an unverified or partial import blocks removal and nothing launches")
{
    RemovalFixture fixture;
    auto detection = fixture.detected();
    auto service = fixture.service();
    auto evidence = fixture.verifiedEvidence(*detection.install);
    evidence.migrationStatus = "needs_review";
    const auto gate = service.canOfferRemoval(evidence, detection);
    CHECK_FALSE(gate.allowed);
    CHECK(hasBlocker(gate, RemovalBlocker::ImportNotVerified));
    const auto outcome = service.requestLegacyRemoval(*detection.install, evidence);
    CHECK(outcome.status == RemovalStatus::Blocked);
    CHECK(fixture.system->launches.empty());
}

TEST_CASE("evidence from a different source blocks removal")
{
    RemovalFixture fixture;
    auto detection = fixture.detected();
    auto service = fixture.service();
    auto evidence = fixture.verifiedEvidence(*detection.install);
    evidence.sourceFingerprint = "1.3.12:9:different";
    const auto gate = service.canOfferRemoval(evidence, detection);
    CHECK_FALSE(gate.allowed);
    CHECK(hasBlocker(gate, RemovalBlocker::SourceMismatch));
}

TEST_CASE("serious import failures and missing re-verification block removal")
{
    RemovalFixture fixture;
    auto detection = fixture.detected();
    auto service = fixture.service();

    auto serious = fixture.verifiedEvidence(*detection.install);
    serious.hasSeriousFailures = true;
    CHECK(hasBlocker(service.canOfferRemoval(serious, detection), RemovalBlocker::SeriousImportIssues));

    auto stale = fixture.verifiedEvidence(*detection.install);
    stale.reVerified = false;
    const auto gate = service.canOfferRemoval(stale, detection);
    CHECK_FALSE(gate.allowed);
    CHECK(hasBlocker(gate, RemovalBlocker::ReverificationRequired));
}

TEST_CASE("an already recorded removal blocks a second one")
{
    RemovalFixture fixture;
    auto detection = fixture.detected();
    auto service = fixture.service();
    auto evidence = fixture.verifiedEvidence(*detection.install);
    evidence.legacyRemoved = true;
    CHECK(hasBlocker(service.canOfferRemoval(evidence, detection), RemovalBlocker::AlreadyRemoved));
}

TEST_CASE("the legacy process is recognised by full path, never by basename")
{
    RemovalFixture fixture;
    auto detection = fixture.detected();
    auto service = fixture.service();
    // The C++ app itself is running under the same executable name.
    fixture.system->processes = {fixture.ownApp / "SkyAutoPiano.exe"};
    CHECK_FALSE(service.isLegacyRunning(*detection.install));
    CHECK(service.canOfferRemoval(fixture.verifiedEvidence(*detection.install), detection).allowed);

    // The Electron executable at its own path blocks removal, and the
    // service has no way to kill it: the request is refused, nothing runs.
    fixture.system->processes = {fixture.executable()};
    CHECK(service.isLegacyRunning(*detection.install));
    const auto gate = service.canOfferRemoval(fixture.verifiedEvidence(*detection.install), detection);
    CHECK(hasBlocker(gate, RemovalBlocker::LegacyRunning));
    const auto outcome = service.requestLegacyRemoval(*detection.install,
                                                      fixture.verifiedEvidence(*detection.install));
    CHECK(outcome.status == RemovalStatus::Blocked);
    CHECK(fixture.system->launches.empty());
}

TEST_CASE("nothing launches before an explicit removal request")
{
    // The confirmation dialog lives in the UI: cancelling it simply means
    // requestLegacyRemoval is never called. Detection and gating alone
    // must be side-effect free.
    RemovalFixture fixture;
    auto detection = fixture.detected();
    auto service = fixture.service();
    (void)service.canOfferRemoval(fixture.verifiedEvidence(*detection.install), detection);
    CHECK(fixture.system->launches.empty());
    CHECK_FALSE(service.removalInProgress());
}

TEST_CASE("removal launches the official uninstaller once and completes on post-checks")
{
    RemovalFixture fixture;
    auto detection = fixture.detected();
    auto service = fixture.service();
    const auto evidence = fixture.verifiedEvidence(*detection.install);

    const auto outcome = service.requestLegacyRemoval(*detection.install, evidence);
    REQUIRE(outcome.status == RemovalStatus::Launched);
    REQUIRE(fixture.system->launches.size() == 1);
    CHECK(std::filesystem::equivalent(fixture.system->launches.front().first, fixture.uninstaller()));
    CHECK(fixture.system->launches.front().second.empty());

    // A double click cannot spawn a second uninstaller.
    const auto again = service.requestLegacyRemoval(*detection.install, evidence);
    CHECK(again.status == RemovalStatus::AlreadyInProgress);
    CHECK(fixture.system->launches.size() == 1);

    // While the uninstaller is still running there is no outcome yet.
    fixture.system->processes = {fixture.uninstaller()};
    CHECK_FALSE(service.pollRemoval().has_value());

    // Uninstaller exited, registry entry gone, library intact: success is
    // decided by the post-checks, not by an exit code.
    fixture.system->processes = {};
    fixture.system->entries.clear();
    const auto done = service.pollRemoval();
    REQUIRE(done.has_value());
    CHECK(done->status == RemovalStatus::Completed);
    CHECK(done->registryEntryGone);
    CHECK(done->libraryIntact);
    CHECK_FALSE(service.removalInProgress());
}

TEST_CASE("a cancelled uninstaller leaves the entry and reports incomplete, retry stays safe")
{
    RemovalFixture fixture;
    auto detection = fixture.detected();
    auto service = fixture.service();
    const auto evidence = fixture.verifiedEvidence(*detection.install);

    REQUIRE(service.requestLegacyRemoval(*detection.install, evidence).status == RemovalStatus::Launched);
    // User pressed Cancel in the Inno UI: process gone, entry still there.
    const auto done = service.pollRemoval();
    REQUIRE(done.has_value());
    CHECK(done->status == RemovalStatus::Incomplete);
    CHECK_FALSE(done->registryEntryGone);

    // Retry is allowed and launches again.
    REQUIRE(service.requestLegacyRemoval(*detection.install, evidence).status == RemovalStatus::Launched);
    CHECK(fixture.system->launches.size() == 2);
    fixture.system->entries.clear();
    const auto retried = service.pollRemoval();
    REQUIRE(retried.has_value());
    CHECK(retried->status == RemovalStatus::Completed);
}

TEST_CASE("a failed library check after removal is not reported as success")
{
    RemovalFixture fixture;
    fixture.libraryIntact = false;
    auto detection = fixture.detected();
    auto service = fixture.service();
    REQUIRE(service.requestLegacyRemoval(*detection.install, fixture.verifiedEvidence(*detection.install)).status ==
            RemovalStatus::Launched);
    fixture.system->entries.clear();
    const auto done = service.pollRemoval();
    REQUIRE(done.has_value());
    CHECK(done->status == RemovalStatus::Failed);
    CHECK(done->registryEntryGone);
    CHECK_FALSE(done->libraryIntact);
}

TEST_CASE("a failed launch is reported and can be retried")
{
    RemovalFixture fixture;
    fixture.system->launchResult = false;
    auto detection = fixture.detected();
    auto service = fixture.service();
    const auto evidence = fixture.verifiedEvidence(*detection.install);
    CHECK(service.requestLegacyRemoval(*detection.install, evidence).status == RemovalStatus::LaunchFailed);
    CHECK_FALSE(service.removalInProgress());
    fixture.system->launchResult = true;
    CHECK(service.requestLegacyRemoval(*detection.install, evidence).status == RemovalStatus::Launched);
}

TEST_CASE("the removed state persists separately from the import evidence")
{
    ensureCoreApp();
    TempDir dir;
    {
        UiStateService state(dir.path());
        state.load();
        state.setLegacyMigration("verified", "1.3.13:1:99", OrderedJson::object());
        CHECK_FALSE(state.legacyRemoved());
        state.setLegacyRemoved();
    }
    {
        UiStateService state(dir.path());
        state.load();
        CHECK(state.legacyRemoved());
        // The Phase 1 evidence survives the removal record untouched.
        CHECK(state.legacyMigrationStatus() == "verified");
        CHECK(state.legacyMigration().value("sourceFingerprint", std::string()) == "1.3.13:1:99");
    }
}
