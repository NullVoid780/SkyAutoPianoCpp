#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <fstream>

#include "core/text_file.hpp"
#include "qt_test_support.hpp"
#include "services/library/chromium_local_storage.hpp"
#include "services/library/legacy_import.hpp"
#include "services/library/library_service.hpp"
#include "services/library/playlist_service.hpp"

using namespace sky;

namespace {

void writeFile(const std::filesystem::path &file, const std::string &text)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

std::filesystem::path makeElectronInstall(const std::filesystem::path &root)
{
    const auto app = root / "resources" / "app";
    writeFile(app / "package.json", R"({"name": "SkyAutoPiano", "version": "1.3.13", "main": "src/main/app.js"})");
    writeFile(app / "data" / "listSheet.json", R"([
        {"name": "Rubia", "author": "Studio", "transcribedBy": "A", "bpm": 68, "keyMap": "rubia.json"},
        {"name": "Lullaby", "author": "", "transcribedBy": "B", "bpm": 90, "keyMap": "lullaby.json"},
        {"name": "rubia", "author": "studio", "transcribedBy": "a", "bpm": 68, "keyMap": "rubia-copy.json"},
        {"name": "Missing", "author": "", "transcribedBy": "", "bpm": 90, "keyMap": "missing.json"}
    ])");
    writeFile(app / "data" / "rubia.json", R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
    writeFile(app / "data" / "lullaby.json", R"({"version":2,"keyMap":{"0":["u"]},"songNotes":[]})");
    return app / "data";
}

}  // namespace

TEST_CASE("an Electron install is recognised from its root, app or data folder")
{
    TempDir temp;
    const auto data = makeElectronInstall(temp.path() / "SkyAutoPiano-win32-x64");
    for (const auto &folder : {temp.path() / "SkyAutoPiano-win32-x64", data.parent_path(), data}) {
        const auto install = inspectLegacyFolder(folder);
        REQUIRE(install);
        CHECK(install->version == "1.3.13");
        CHECK(install->songCount == 4);
        CHECK(std::filesystem::equivalent(install->dataDirectory, data));
    }
}

TEST_CASE("folders that are not the Electron app are ignored")
{
    TempDir temp;
    writeFile(temp.path() / "other" / "package.json", R"({"name": "something-else", "main": "index.js"})");
    writeFile(temp.path() / "other" / "data" / "listSheet.json", "[]");
    CHECK_FALSE(inspectLegacyFolder(temp.path() / "other"));
    CHECK_FALSE(inspectLegacyFolder(temp.path() / "empty"));
}

TEST_CASE("search finds installs below a root but never the app's own data folder")
{
    TempDir temp;
    const auto data = makeElectronInstall(temp.path() / "Downloads" / "SkyAutoPiano-win32-x64");
    const auto found = findLegacyInstalls(temp.path() / "native-data", {temp.path() / "Downloads"}, false);
    REQUIRE(found.size() == 1);
    CHECK(std::filesystem::equivalent(found.front().dataDirectory, data));
    CHECK(findLegacyInstalls(data, {temp.path() / "Downloads"}, false).empty());
}

TEST_CASE("the import plan skips songs already in the library and duplicates")
{
    TempDir temp;
    const auto data = makeElectronInstall(temp.path() / "old");
    const auto install = inspectLegacyFolder(temp.path() / "old");
    REQUIRE(install);
    const std::set<std::string> existing = {legacySongKey(OrderedJson::parse(
        R"({"name": " Lullaby ", "author": "", "transcribedBy": "b"})"))};
    auto plan = planLegacyImport(*install, existing);
    REQUIRE(plan.ok());
    REQUIRE(plan.value().rows.size() == 2);
    CHECK(plan.value().rows[0]["name"] == "Rubia");
    CHECK(plan.value().rows[1]["name"] == "Missing");
    CHECK(plan.value().alreadyPresent == 2);
}

TEST_CASE("importing copies payloads, keeps the old install and reports missing files")
{
    ensureCoreApp();
    TempDir temp;
    const auto data = makeElectronInstall(temp.path() / "old");
    const auto nativeData = temp.path() / "native";
    writeFile(nativeData / "rubia.json", R"({"version":2,"keyMap":{},"songNotes":[]})");
    PlaylistService playlists(nativeData);
    LibraryService library(nativeData, playlists);
    playlists.load();
    library.load();

    auto plan = planLegacyImport(*inspectLegacyFolder(temp.path() / "old"), library.legacySongKeys());
    REQUIRE(plan.ok());
    bool finished = false;
    LegacyImportSummary summary;
    library.importLegacyRows(plan.value().install.dataDirectory, plan.value().rows, plan.value().alreadyPresent,
                             [&](LegacyImportSummary result) {
                                 summary = result;
                                 finished = true;
                             });
    REQUIRE(waitUntil([&] { return finished; }));
    CHECK(summary.imported == 2);
    CHECK(summary.failed == 1);
    REQUIRE(library.size() == 2);
    CHECK(library.entry(0)->payloadFile() != "rubia.json");
    CHECK(library.entry(1)->payloadFile() == "lullaby.json");
    CHECK(std::filesystem::exists(nativeData / library.entry(0)->payloadFile()));
    CHECK(std::filesystem::exists(data / "rubia.json"));

    LibraryService reloaded(nativeData, playlists);
    reloaded.load();
    CHECK(reloaded.size() == 2);
    CHECK(planLegacyImport(*inspectLegacyFolder(temp.path() / "old"), reloaded.legacySongKeys()).value().rows.size() == 1);
}

TEST_CASE("the plan carries old settings, playlists, favorites, recent plays and theme")
{
    TempDir temp;
    const auto data = makeElectronInstall(temp.path() / "old");
    writeFile(temp.path() / "old" / "resources" / "app" / "config" / "config.json",
              R"({"panel": {"speed": 1.5, "playbackMode": "strict"}})");
    writeFile(data / "playlists.json", R"([{"id": "p1", "name": "Evening", "sheetKeys": ["rubia.json"]}])");
    const auto storage = temp.path() / "leveldb";
    std::filesystem::create_directories(storage);
    const auto key = [](const std::string &name) { return std::string("_file://") + std::string(1, '\0') + "\x01" + name; };
    const auto record = [](const std::string &k, const std::string &v) {
        std::string batch(8, '\0');
        batch[0] = 1;
        batch += std::string("\x01\x00\x00\x00", 4);
        batch.push_back('\x01');
        batch.push_back(static_cast<char>(k.size()));
        batch += k;
        batch.push_back(static_cast<char>(v.size() + 1));
        batch += "\x01" + v;
        std::string header(4, '\0');
        header.push_back(static_cast<char>(batch.size() & 0xff));
        header.push_back(static_cast<char>(batch.size() >> 8));
        header.push_back('\x01');
        return header + batch;
    };
    writeFile(storage / "000003.log", record(key("favorites"), R"(["Rubia"])") +
                                          record(key("recentPlays"), R"(["Lullaby","Rubia"])") +
                                          record(key("theme"), "dark"));

    auto plan = planLegacyImport(*inspectLegacyFolder(temp.path() / "old"), {}, storage);
    REQUIRE(plan.ok());
    const LegacyExtras &extras = plan.value().extras;
    REQUIRE(extras.config);
    CHECK((*extras.config)["panel"]["playbackMode"] == "strict");
    REQUIRE(extras.playlists.size() == 1);
    CHECK(extras.playlists[0]["name"] == "Evening");
    CHECK(extras.favorites == std::vector<std::string>{"Rubia"});
    CHECK(extras.recentPlays == std::vector<std::string>{"Lullaby", "Rubia"});
    REQUIRE(extras.theme);
    CHECK(*extras.theme == "dark");
    CHECK(plan.value().allRows.size() == 4);
}
TEST_CASE("dry run against this machine's previous install", "[.manual]")
{
    const auto installs = findLegacyInstalls(std::filesystem::temp_directory_path() / "no-own-data",
                                             defaultLegacySearchRoots());
    for (const auto &install : installs) {
        auto plan = planLegacyImport(install, {}, electronLocalStorageDirectory());
        REQUIRE(plan.ok());
        const auto &extras = plan.value().extras;
        std::printf("install %s v%s: songs=%zu playlists=%zu favorites=%zu recent=%zu theme=%s config=%s\n",
                    install.root.string().c_str(), install.version.c_str(), plan.value().rows.size(),
                    extras.playlists.size(), extras.favorites.size(), extras.recentPlays.size(),
                    extras.theme.value_or("-").c_str(), extras.config ? "yes" : "no");
    }
}

TEST_CASE("a same-key sheet with different payload bytes is a conflict, not a duplicate")
{
    TempDir temp;
    makeElectronInstall(temp.path() / "old");
    const auto install = inspectLegacyFolder(temp.path() / "old");
    REQUIRE(install);
    const auto destData = temp.path() / "native";
    writeFile(destData / "existing-rubia.json", R"({"version":2,"keyMap":{"0":["h"]},"songNotes":[]})");
    const OrderedJson existingRow =
        OrderedJson::parse(R"({"name":"Rubia","author":"Studio","transcribedBy":"A","keyMap":"existing-rubia.json"})");
    const std::map<std::string, std::string> payloads{{legacySongKey(existingRow), "existing-rubia.json"}};
    const std::set<std::string> keys{legacySongKey(existingRow)};
    auto plan = planLegacyImport(*install, keys, {}, destData, payloads);
    REQUIRE(plan.ok());
    CHECK(plan.value().conflicts.size() == 1);
    CHECK(plan.value().conflicts.front().sourceFile == "rubia.json");
    CHECK(plan.value().conflicts.front().destinationFile == "existing-rubia.json");
    CHECK(plan.value().rows.size() == 2);
    CHECK(plan.value().alreadyPresent == 1);
}

TEST_CASE("a same-key sheet with identical payload bytes is already present")
{
    TempDir temp;
    makeElectronInstall(temp.path() / "old");
    const auto install = inspectLegacyFolder(temp.path() / "old");
    REQUIRE(install);
    const auto destData = temp.path() / "native";
    writeFile(destData / "existing-rubia.json", R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
    const OrderedJson existingRow =
        OrderedJson::parse(R"({"name":"Rubia","author":"Studio","transcribedBy":"A","keyMap":"existing-rubia.json"})");
    const std::map<std::string, std::string> payloads{{legacySongKey(existingRow), "existing-rubia.json"}};
    const std::set<std::string> keys{legacySongKey(existingRow)};
    auto plan = planLegacyImport(*install, keys, {}, destData, payloads);
    REQUIRE(plan.ok());
    CHECK(plan.value().conflicts.empty());
    CHECK(plan.value().alreadyPresent == 2);
    CHECK(plan.value().rows.size() == 2);
}

TEST_CASE("verification validates copied payloads and referenced payloads")
{
    TempDir temp;
    const auto source = temp.path() / "source";
    const auto dest = temp.path() / "dest";
    writeFile(source / "a.json", R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
    writeFile(dest / "a.json", R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
    const auto ok = verifyImportedSheets(source, dest, {{"a.json", "a.json"}}, {"a.json"});
    CHECK(ok.verified);
    CHECK(ok.checked == 2);
    CHECK(ok.failed == 0);
    const auto brokenRef = verifyImportedSheets(source, dest, {{"a.json", "a.json"}}, {"missing.json"});
    CHECK_FALSE(brokenRef.verified);
    CHECK(brokenRef.failed == 1);
    writeFile(dest / "a.json", R"({"version":2,"keyMap":{"0":["h"]},"songNotes":[]})");
    const auto mismatch = verifyImportedSheets(source, dest, {{"a.json", "a.json"}}, {});
    CHECK_FALSE(mismatch.verified);
    CHECK(mismatch.failed == 1);
}

TEST_CASE("the destination snapshot keeps the first pre-import state for a source")
{
    TempDir temp;
    const auto data = temp.path() / "native-data";
    const auto config = temp.path() / "native-config";
    writeFile(data / "listSheet.json", "v1");
    writeFile(config / "config.json", "c1");
    const auto first = snapshotDestinationFiles(data, config, "source-1");
    REQUIRE_FALSE(first.empty());
    CHECK(readTextFile(first / "listSheet.json").value() == "v1");
    CHECK(readTextFile(first / "config.json").value() == "c1");
    writeFile(data / "listSheet.json", "v2");
    const auto sameSource = snapshotDestinationFiles(data, config, "source-1");
    CHECK(readTextFile(sameSource / "listSheet.json").value() == "v1");
    const auto otherSource = snapshotDestinationFiles(data, config, "source-2");
    CHECK(readTextFile(otherSource / "listSheet.json").value() == "v2");
}

TEST_CASE("import leaves every source fixture byte-for-byte unchanged")
{
    ensureCoreApp();
    TempDir temp;
    const auto data = makeElectronInstall(temp.path() / "old");
    const auto beforeList = readTextFile(data / "listSheet.json").value();
    const auto beforePayload = readTextFile(data / "rubia.json").value();
    const auto nativeData = temp.path() / "native";
    PlaylistService playlists(nativeData);
    LibraryService library(nativeData, playlists);
    playlists.load();
    library.load();
    auto plan = planLegacyImport(*inspectLegacyFolder(temp.path() / "old"), library.legacySongKeys());
    REQUIRE(plan.ok());
    bool finished = false;
    library.importLegacyRows(plan.value().install.dataDirectory, plan.value().rows, plan.value().alreadyPresent,
                             [&](LegacyImportSummary) { finished = true; });
    REQUIRE(waitUntil([&] { return finished; }));
    CHECK(readTextFile(data / "listSheet.json").value() == beforeList);
    CHECK(readTextFile(data / "rubia.json").value() == beforePayload);
}

TEST_CASE("the structured import result serializes honest counts")
{
    LegacyImportResult result;
    result.sourceFingerprint = "1.3.13:4:12345";
    result.sheets.planned = 4;
    result.sheets.copied = 2;
    result.sheets.conflicts = 1;
    result.sheets.failed = 1;
    result.verificationStatus = "needs_review";
    const OrderedJson json = result.toJson();
    CHECK(json["version"] == 1);
    CHECK(json["sheets"]["copied"] == 2);
    CHECK(json["sheets"]["conflicts"] == 1);
    CHECK(json["verification"]["status"] == "needs_review");
    CHECK(json["sourceAccess"] == "read-only");
    CHECK(legacySourceFingerprint(LegacyInstall{{}, {}, "1.3.13", 4}).rfind("1.3.13:4:", 0) == 0);
}

TEST_CASE("reconciliation expects the deduped source rows, not the raw duplicates")
{
    TempDir temp;
    const auto app = temp.path() / "old" / "resources" / "app";
    writeFile(app / "package.json", R"({"name": "SkyAutoPiano", "version": "1.3.13", "main": "src/main/app.js"})");
    // One song listed twice under the same name, author and transcriber,
    // pointing at two different payload files: the importer copies the
    // first and counts the second as already present.
    writeFile(app / "data" / "listSheet.json", R"([
        {"name": "Servant", "author": "Unknown", "transcribedBy": "Unknown", "bpm": 90, "keyMap": "servant-a.json"},
        {"name": "Servant", "author": "Unknown", "transcribedBy": "Unknown", "bpm": 90, "keyMap": "servant-b.json"},
        {"name": "Lullaby", "author": "", "transcribedBy": "B", "bpm": 90, "keyMap": "lullaby.json"}
    ])");
    writeFile(app / "data" / "servant-a.json", R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
    writeFile(app / "data" / "lullaby.json", R"({"version":2,"keyMap":{"0":["u"]},"songNotes":[]})");
    const auto install = inspectLegacyFolder(temp.path() / "old");
    REQUIRE(install);

    // The destination library holds the first Servant payload, exactly as
    // the importer left it.
    const auto dest = temp.path() / "native";
    writeFile(dest / "servant.json", R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
    writeFile(dest / "lullaby.json", R"({"version":2,"keyMap":{"0":["u"]},"songNotes":[]})");
    const OrderedJson servantRow =
        OrderedJson::parse(R"({"name": "Servant", "author": "Unknown", "transcribedBy": "Unknown"})");
    const OrderedJson lullabyRow = OrderedJson::parse(R"({"name": "Lullaby", "author": "", "transcribedBy": "B"})");
    const std::set<std::string> keys = {legacySongKey(servantRow), legacySongKey(lullabyRow)};
    const std::map<std::string, std::string> existingPayloads = {
        {legacySongKey(servantRow), "servant.json"}, {legacySongKey(lullabyRow), "lullaby.json"}};

    SECTION("the duplicate payload differs from the first")
    {
        writeFile(app / "data" / "servant-b.json", R"({"version":2,"keyMap":{"0":["u","i"]},"songNotes":[1,2,3]})");
    }
    SECTION("the duplicate payload is byte-identical to the first")
    {
        writeFile(app / "data" / "servant-b.json", R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
    }

    auto plan = planLegacyImport(*install, keys, {}, dest, existingPayloads);
    REQUIRE(plan.ok());
    CHECK(plan.value().rows.empty());
    CHECK(plan.value().conflicts.empty());
    CHECK(plan.value().missingPayloads == 0);
    REQUIRE(plan.value().allRows.size() == 3);

    // Build the expected set the way the removal reconciliation does: from
    // the deduped rows, so the skipped duplicate is satisfied by the
    // surviving row instead of being expected byte-for-byte.
    const std::vector<OrderedJson> deduped = dedupedLegacyRows(plan.value().allRows);
    REQUIRE(deduped.size() == 2);
    CHECK(deduped[0].value("keyMap", std::string()) == "servant-a.json");
    std::vector<std::pair<std::string, std::string>> expected;
    std::vector<std::string> referenced;
    for (const OrderedJson &row : deduped) {
        const auto found = existingPayloads.find(legacySongKey(row));
        REQUIRE(found != existingPayloads.end());
        expected.emplace_back(row.value("keyMap", std::string()), found->second);
        referenced.push_back(found->second);
    }
    const LegacyVerification check = verifyImportedSheets(install->dataDirectory, dest, expected, referenced);
    CHECK(check.verified);
    CHECK(check.failed == 0);
    CHECK(check.checked == 4);
}

TEST_CASE("reconciliation still blocks when the library payload is not the imported one")
{
    TempDir temp;
    const auto app = temp.path() / "old" / "resources" / "app";
    writeFile(app / "package.json", R"({"name": "SkyAutoPiano", "version": "1.3.13", "main": "src/main/app.js"})");
    writeFile(app / "data" / "listSheet.json", R"([
        {"name": "Servant", "author": "Unknown", "transcribedBy": "Unknown", "bpm": 90, "keyMap": "servant-a.json"},
        {"name": "Servant", "author": "Unknown", "transcribedBy": "Unknown", "bpm": 90, "keyMap": "servant-b.json"}
    ])");
    writeFile(app / "data" / "servant-a.json", R"({"version":2,"keyMap":{"0":["y"]},"songNotes":[]})");
    writeFile(app / "data" / "servant-b.json", R"({"version":2,"keyMap":{"0":["u","i"]},"songNotes":[1,2,3]})");
    const auto install = inspectLegacyFolder(temp.path() / "old");
    REQUIRE(install);

    // The library holds a different arrangement under the same key: the
    // Phase 1 conflict, which must keep blocking the removal gate.
    const auto dest = temp.path() / "native";
    writeFile(dest / "servant.json", R"({"version":2,"keyMap":{"0":["h"]},"songNotes":[]})");
    const OrderedJson servantRow =
        OrderedJson::parse(R"({"name": "Servant", "author": "Unknown", "transcribedBy": "Unknown"})");
    const std::set<std::string> keys = {legacySongKey(servantRow)};
    const std::map<std::string, std::string> existingPayloads = {{legacySongKey(servantRow), "servant.json"}};

    auto plan = planLegacyImport(*install, keys, {}, dest, existingPayloads);
    REQUIRE(plan.ok());
    REQUIRE(plan.value().conflicts.size() == 1);
    CHECK(plan.value().conflicts.front().sourceFile == "servant-a.json");
    CHECK(plan.value().conflicts.front().destinationFile == "servant.json");
}
