#include <catch2/catch_test_macros.hpp>

#include <fstream>

#include "core/base64.hpp"
#include "core/text_file.hpp"
#include "qt_test_support.hpp"
#include "services/library/library_service.hpp"
#include "services/library/playlist_service.hpp"

using namespace sky;

namespace {

void writeFile(const std::filesystem::path &file, const std::string &text)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << text;
}

struct Fixture {
    Fixture() : playlists(dir.path()), library(dir.path(), playlists)
    {
        playlists.load();
        library.load();
        QObject::connect(&library, &LibraryService::importFinished, [this](const ImportSummary &result) {
            summary = result;
            finished = true;
        });
    }

    ImportSummary importAndWait(std::vector<std::filesystem::path> files)
    {
        finished = false;
        library.importFiles(std::move(files));
        REQUIRE(waitUntil([this] { return finished; }));
        return summary;
    }

    bool appReady = (ensureCoreApp(), true);
    TempDir dir;
    PlaylistService playlists;
    LibraryService library;
    ImportSummary summary;
    bool finished = false;
};

}  // namespace

TEST_CASE("payload file names reproduce the original generator")
{
    CHECK(LibraryService::payloadFileName("Song title", 123) == base64Encode("123Song-title") + ".json");
    CHECK(LibraryService::payloadFileName("Ca\xC3\xA7\xF0\x9F\x8E\xB9", 7) == base64Encode("7Ca---") + ".json");
}

TEST_CASE("a missing list is created empty")
{
    Fixture f;
    CHECK(f.library.size() == 0);
    CHECK(readTextFile(f.dir.path() / "listSheet.json").value() == "[]");
}

TEST_CASE("importing a sheet writes the payload and appends the row")
{
    Fixture f;
    const auto source = f.dir.path() / "in" / "Rubia.json";
    writeFile(source,
              R"([{"name":"Rubia","author":"Sky Music Studio","bpm":68,"pitchLevel":0,"songNotes":[{"time":0,"key":"Key5"},{"time":882,"key":"Key2","hold":300}]}])");
    const ImportSummary summary = f.importAndWait({source});
    CHECK(summary.total == 1);
    CHECK(summary.succeeded == 1);
    CHECK(LibraryService::importToast(summary).message == "Sheet imported successfully.");

    REQUIRE(f.library.size() == 1);
    const SheetEntry &row = *f.library.entry(0);
    CHECK(row.name() == "Rubia");
    CHECK(row.author() == "Sky Music Studio");
    CHECK(row.transcribedBy() == "Unknown");
    CHECK(row.bpm() == 68.0);
    CHECK(row.bitsPerPage() == 16.0);
    CHECK(row.hasHoldNotes());
    CHECK(row.isComposed());
    const std::string payloadText = readTextFile(f.dir.path() / row.payloadFile()).value();
    CHECK(payloadText ==
          R"({"version":2,"keyMap":{"0":["h"],"882":["i"],"2000":[]},"songNotes":[{"time":0,"key":"1Key5"},{"time":882,"key":"1Key2","hold":300}]})");

    const std::string listText = readTextFile(f.dir.path() / "listSheet.json").value();
    CHECK(listText.find("\"keyMap\": \"" + row.payloadFile() + "\"") != std::string::npos);

    auto payload = f.library.payload(0);
    REQUIRE(payload.ok());
    CHECK(payload.value()->hasHoldNotes);
    CHECK(payload.value()->playbackNotes->size() == 2);
}

TEST_CASE("multi-file import reports successes and the first failure")
{
    Fixture f;
    const auto good = f.dir.path() / "in" / "good.txt";
    const auto bad = f.dir.path() / "in" / "bad.json";
    writeFile(good, "120\nA1 B2");
    writeFile(bad, R"([{"songNotes":[{"time":0,"key":"Key99"}]}])");
    const ImportSummary summary = f.importAndWait({good, bad});
    CHECK(summary.succeeded == 1);
    REQUIRE(summary.failures.size() == 1);
    const Toast toast = LibraryService::importToast(summary);
    CHECK(toast.type == ToastType::Success);
    CHECK(toast.message ==
          "Import complete. Success: 1. Failed: 1. bad.json: Note 1 uses key 99, but supported keys are 0–14.");

    const ImportSummary failedOnly = f.importAndWait({bad});
    CHECK(LibraryService::importToast(failedOnly).type == ToastType::Error);
    CHECK(LibraryService::importToast(failedOnly).message == "Note 1 uses key 99, but supported keys are 0–14.");
}

TEST_CASE("payloads load asynchronously and are cached")
{
    Fixture f;
    const auto source = f.dir.path() / "in" / "a.txt";
    writeFile(source, "240\nA1 A2");
    f.importAndWait({source});
    bool done = false;
    Result<std::shared_ptr<const StoredSheet>> loaded = Error{};
    f.library.loadPayloadAsync(0, [&](Result<std::shared_ptr<const StoredSheet>> result) {
        loaded = std::move(result);
        done = true;
    });
    REQUIRE(waitUntil([&] { return done; }));
    REQUIRE(loaded.ok());
    CHECK(loaded.value()->keyMap.size() == 3);
    CHECK(f.library.payload(0).value() == loaded.value());
}

TEST_CASE("an unreadable payload reports an error")
{
    Fixture f;
    writeFile(f.dir.path() / "listSheet.json", R"([{"name":"Ghost","keyMap":"missing.json"}])");
    f.library.load();
    REQUIRE(f.library.size() == 1);
    CHECK_FALSE(f.library.payload(0).ok());
}

TEST_CASE("deleting a sheet removes its payload and playlist membership")
{
    Fixture f;
    const auto a = f.dir.path() / "in" / "a.txt";
    const auto b = f.dir.path() / "in" / "b.txt";
    writeFile(a, "240\nA1");
    writeFile(b, "240\nB1");
    f.importAndWait({a, b});
    REQUIRE(f.library.size() == 2);
    const std::string keyA = f.library.entry(0)->payloadFile();
    const std::string keyB = f.library.entry(1)->payloadFile();
    const std::string id = f.playlists.create("P").value().id;
    (void)f.playlists.addSheets(id, {keyA, keyB});

    REQUIRE(f.library.deleteSheet(0).ok());
    CHECK(f.library.size() == 1);
    CHECK_FALSE(std::filesystem::exists(f.dir.path() / keyA));
    CHECK(f.playlists.find(id)->sheetKeys == std::vector<std::string>{keyB});

    const DeleteSummary summary = f.library.deleteSheets({keyB});
    CHECK(summary.deletedKeys.size() == 1);
    CHECK(f.library.size() == 0);
    CHECK(f.playlists.find(id)->sheetKeys.empty());
    CHECK(LibraryService::deleteToast(summary).message == "Deleted 1 sheet.");
}

TEST_CASE("bulk delete refuses payload paths outside the data folder")
{
    Fixture f;
    writeFile(f.dir.path() / "listSheet.json", R"([{"name":"Evil","keyMap":"../outside.json"},{"name":"","keyMap":""}])");
    f.library.load();
    const DeleteSummary summary = f.library.deleteSheets({"../outside.json", ""});
    CHECK(summary.deletedKeys.empty());
    CHECK(summary.failedNames == std::vector<std::string>{"Evil", ""});
    CHECK(f.library.size() == 2);
    const Toast toast = LibraryService::deleteToast(summary);
    CHECK(toast.type == ToastType::Error);
    CHECK(toast.message == "Unable to delete the selected sheets. Evil");
    CHECK(LibraryService::deleteConfirmation(3) == "Delete 3 selected sheets? This cannot be undone.");
    CHECK(LibraryService::deleteConfirmation(1) == "Delete 1 selected sheet? This cannot be undone.");
}

TEST_CASE("store imports reject duplicates and encrypted sheets")
{
    Fixture f;
    const OrderedJson sheet = OrderedJson::parse(
        R"({"name":"Store","author":"A","transcribedBy":"T","isComposed":true,"bpm":100,"bitsPerPage":15,"pitchLevel":0,"isEncrypted":false,"songNotes":[{"time":0,"key":"1Key3"}],"hasHoldNotes":false})");
    const StoreSource source{"sky-sheet-store", "11111111-1111-4111-8111-111111111111", "https://skysheet.store/sheets/x"};

    bool done = false;
    Result<void> result = Error{};
    f.library.importStoreSheet(sheet, source, [&](Result<void> r) {
        result = r;
        done = true;
    });
    REQUIRE(waitUntil([&] { return done; }));
    REQUIRE(result.ok());
    REQUIRE(f.library.size() == 1);
    CHECK(f.library.entry(0)->source() == "sky-sheet-store");
    CHECK(f.library.entry(0)->sourceId() == source.sourceId);

    done = false;
    f.library.importStoreSheet(sheet, source, [&](Result<void> r) {
        result = r;
        done = true;
    });
    REQUIRE(done);
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().userMessage == "This sheet is already in your library.");

    OrderedJson encrypted = sheet;
    encrypted["isEncrypted"] = true;
    f.library.importStoreSheet(encrypted, StoreSource{"sky-sheet-store", "other", ""}, [&](Result<void> r) { result = r; });
    CHECK(result.error().userMessage == "Sheet has been encrypted!");
}

TEST_CASE("editor saves update the row and the payload")
{
    Fixture f;
    const auto a = f.dir.path() / "in" / "a.txt";
    writeFile(a, "240\nA1");
    f.importAndWait({a});
    REQUIRE(f.library.updateEntryField(0, "bpm", 90).ok());
    CHECK(f.library.entry(0)->bpm() == 90.0);

    const KeyMap edited({{0.0, {"u"}}, {1000.0, {}}});
    REQUIRE(f.library.saveEditedKeyMap(0, edited).ok());
    const std::string text = readTextFile(f.dir.path() / f.library.entry(0)->payloadFile()).value();
    CHECK(text == R"({"version":2,"keyMap":{"0":["u"],"1000":[]},"songNotes":[{"time":0,"key":"1Key1"}]})");
}
