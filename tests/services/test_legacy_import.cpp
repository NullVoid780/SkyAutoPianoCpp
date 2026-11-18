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
