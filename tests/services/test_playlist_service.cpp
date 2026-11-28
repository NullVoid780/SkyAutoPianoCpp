#include <catch2/catch_test_macros.hpp>

#include <fstream>

#include "core/text_file.hpp"
#include "qt_test_support.hpp"
#include "services/library/playlist_service.hpp"

using namespace sky;

TEST_CASE("creating playlists validates and persists names") {
    ensureCoreApp();
    TempDir dir;
    PlaylistService service(dir.path());
    service.load();

    auto empty = service.create("   ");
    REQUIRE_FALSE(empty.ok());
    CHECK(empty.error().userMessage == "Enter a playlist name.");

    auto created = service.create("  Favourites  ");
    REQUIRE(created.ok());
    CHECK(created.value().name == "Favourites");
    CHECK(created.value().id.rfind("playlist-", 0) == 0);
    CHECK(created.value().id.size() > std::string("playlist-0-").size());

    auto duplicate = service.create("FAVOURITES");
    REQUIRE_FALSE(duplicate.ok());
    CHECK(duplicate.error().userMessage == "A playlist with this name already exists.");

    auto longName = service.create(std::string(100, 'x'));
    REQUIRE(longName.ok());
    CHECK(longName.value().name.size() == 80);

    PlaylistService reloaded(dir.path());
    reloaded.load();
    REQUIRE(reloaded.playlists().size() == 2);
    CHECK(reloaded.playlists()[0].name == "Favourites");
}

TEST_CASE("renaming requires a unique non-empty name") {
    ensureCoreApp();
    TempDir dir;
    PlaylistService service(dir.path());
    service.load();
    const std::string a = service.create("A").value().id;
    REQUIRE(service.create("B").ok());

    auto clash = service.rename(a, "b");
    REQUIRE_FALSE(clash.ok());
    CHECK(clash.error().userMessage == "Enter a unique playlist name.");
    CHECK(service.rename(a, "a").ok());
    CHECK(service.find(a)->name == "a");
}

TEST_CASE("sheets are added, reordered and removed by payload key") {
    ensureCoreApp();
    TempDir dir;
    PlaylistService service(dir.path());
    service.load();
    const std::string id = service.create("Mix").value().id;

    REQUIRE(service.addSheet(id, "one.json").ok());
    auto again = service.addSheet(id, "one.json");
    REQUIRE_FALSE(again.ok());
    CHECK(again.error().userMessage == "This sheet is already in the playlist.");

    auto added = service.addSheets(id, {"one.json", "two.json", "three.json"});
    REQUIRE(added.ok());
    CHECK(added.value() == 2);
    CHECK(PlaylistService::bulkAddMessage(2, "Mix") == "Added 2 sheets to Mix.");
    CHECK(PlaylistService::bulkAddMessage(1, "Mix") == "Added 1 sheet to Mix.");
    CHECK(PlaylistService::bulkAddMessage(0, "Mix") == "All selected sheets are already in Mix.");

    auto missing = service.addSheets("nope", {"x"});
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.error().userMessage == "Create or select a playlist first.");

    CHECK(service.moveSheet(id, "three.json", -1));
    CHECK_FALSE(service.moveSheet(id, "one.json", -1));
    CHECK(service.find(id)->sheetKeys ==
          std::vector<std::string>{"one.json", "three.json", "two.json"});

    REQUIRE(service.removeSheetsEverywhere({"three.json"}).ok());
    REQUIRE(service.removeSheet(id, "one.json").ok());
    CHECK(service.find(id)->sheetKeys == std::vector<std::string>{"two.json"});

    CHECK(service.remove(id));
    CHECK(service.playlists().empty());
}

TEST_CASE("loading drops invalid playlists and duplicate keys") {
    ensureCoreApp();
    TempDir dir;
    {
        std::ofstream out(dir.path() / "playlists.json", std::ios::binary);
        out << R"([{"id":"p1","name":"  One ","sheetKeys":["a","a","",3,"b"]},{"id":"","name":"x"},{"id":"p2"},5])";
    }
    PlaylistService service(dir.path());
    service.load();
    REQUIRE(service.playlists().size() == 1);
    CHECK(service.playlists()[0].name == "One");
    CHECK(service.playlists()[0].sheetKeys == std::vector<std::string>{"a", "b"});
}

TEST_CASE("playlists.json round-trips with four-space indentation") {
    ensureCoreApp();
    TempDir dir;
    PlaylistService service(dir.path());
    service.load();
    const std::string id = service.create("Mix").value().id;
    (void)service.addSheet(id, "k.json");
    const std::string text = readTextFile(dir.path() / "playlists.json").value();
    CHECK(text == "[\n    {\n        \"id\": \"" + id +
                      "\",\n        \"name\": \"Mix\",\n        \"sheetKeys\": [\n            "
                      "\"k.json\"\n        ]\n    }\n]");
}

TEST_CASE(
    "playlist mutations restore in-memory state and suppress changed signal when save fails") {
    ensureCoreApp();
    TempDir dir;
    bool failWrite = false;
    StorageFileOps ops = StorageFileOps::native();
    ops.atomicWrite = [&](const std::filesystem::path &file,
                          std::string_view bytes) -> Result<void> {
        if (failWrite) {
            return Error{"WRITE_FAILED", "Playlist write blocked", "injected"};
        }
        return StorageFileOps::native().atomicWrite(file, bytes);
    };

    PlaylistService service(dir.path(), ops);
    service.load();
    const std::string id = service.create("Mix").value().id;
    REQUIRE(service.addSheets(id, {"a.json", "b.json"}).ok());

    int changedCount = 0;
    QObject::connect(&service, &PlaylistService::changed, [&] { ++changedCount; });

    failWrite = true;
    CHECK_FALSE(service.create("Second").ok());
    CHECK(service.playlists().size() == 1);
    CHECK_FALSE(service.rename(id, "Renamed").ok());
    CHECK(service.find(id)->name == "Mix");
    CHECK_FALSE(service.addSheet(id, "c.json").ok());
    CHECK_FALSE(service.addSheets(id, {"c.json"}).ok());
    CHECK_FALSE(service.removeSheet(id, "a.json").ok());
    CHECK_FALSE(service.moveSheet(id, "b.json", -1));
    CHECK_FALSE(service.removeSheetsEverywhere({"a.json"}).ok());
    CHECK_FALSE(service.remove(id));
    CHECK(service.playlists().size() == 1);
    CHECK(service.find(id)->sheetKeys == std::vector<std::string>{"a.json", "b.json"});
    CHECK(changedCount == 0);
    CHECK(service.lastError().has_value());

    PlaylistService reloaded(dir.path());
    reloaded.load();
    REQUIRE(reloaded.playlists().size() == 1);
    CHECK(reloaded.find(id)->name == "Mix");
    CHECK(reloaded.find(id)->sheetKeys == std::vector<std::string>{"a.json", "b.json"});
}
