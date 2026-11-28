#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <fstream>

#include "codec/sheet_exporter.hpp"
#include "core/base64.hpp"
#include "core/text_file.hpp"
#include "qt_test_support.hpp"
#include "services/library/library_service.hpp"
#include "services/library/playlist_service.hpp"

using namespace sky;

namespace {

void writeFile(const std::filesystem::path &file, const std::string &text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << text;
}

struct Fixture {
    Fixture() : playlists(dir.path()), library(dir.path(), playlists) {
        playlists.load();
        library.load();
        QObject::connect(&library, &LibraryService::importFinished,
                         [this](const ImportSummary &result) {
                             summary = result;
                             finished = true;
                         });
    }

    ImportSummary importAndWait(std::vector<std::filesystem::path> files) {
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

TEST_CASE("payload file names reproduce the original generator") {
    CHECK(LibraryService::payloadFileName("Song title", 123) ==
          base64Encode("123Song-title") + ".json");
    CHECK(LibraryService::payloadFileName("Ca\xC3\xA7\xF0\x9F\x8E\xB9", 7) ==
          base64Encode("7Ca---") + ".json");
}

TEST_CASE("a missing list is created empty") {
    Fixture f;
    CHECK(f.library.size() == 0);
    CHECK(readTextFile(f.dir.path() / "listSheet.json").value() == "[]");
}

TEST_CASE("importing a sheet writes the payload and appends the row") {
    Fixture f;
    const auto source = f.dir.path() / "in" / "Rubia.json";
    writeFile(
        source,
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
    CHECK(
        payloadText ==
        R"({"version":2,"keyMap":{"0":["h"],"882":["i"],"2000":[]},"songNotes":[{"time":0,"key":"1Key5"},{"time":882,"key":"1Key2","hold":300}]})");

    const std::string listText = readTextFile(f.dir.path() / "listSheet.json").value();
    CHECK(listText.find("\"keyMap\": \"" + row.payloadFile() + "\"") != std::string::npos);

    auto payload = f.library.payload(0);
    REQUIRE(payload.ok());
    CHECK(payload.value()->hasHoldNotes);
    CHECK(payload.value()->playbackNotes->size() == 2);
}

TEST_CASE("multi-file import reports successes and the first failure") {
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
    CHECK(toast.message == "Import complete. Success: 1. Failed: 1. bad.json: Note 1 uses key 99, "
                           "but supported keys are 0–14.");

    const ImportSummary failedOnly = f.importAndWait({bad});
    CHECK(LibraryService::importToast(failedOnly).type == ToastType::Error);
    CHECK(LibraryService::importToast(failedOnly).message ==
          "Note 1 uses key 99, but supported keys are 0–14.");
}

TEST_CASE("payloads load asynchronously and are cached") {
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

TEST_CASE("an unreadable payload reports an error") {
    Fixture f;
    writeFile(f.dir.path() / "listSheet.json", R"([{"name":"Ghost","keyMap":"missing.json"}])");
    f.library.load();
    REQUIRE(f.library.size() == 1);
    CHECK_FALSE(f.library.payload(0).ok());
}

TEST_CASE("deleting a sheet removes its payload and playlist membership") {
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

TEST_CASE("bulk delete refuses payload paths outside the data folder") {
    Fixture f;
    writeFile(f.dir.path() / "listSheet.json",
              R"([{"name":"Evil","keyMap":"../outside.json"},{"name":"","keyMap":""}])");
    f.library.load();
    const DeleteSummary summary = f.library.deleteSheets({"../outside.json", ""});
    CHECK(summary.deletedKeys.empty());
    CHECK(summary.failedNames == std::vector<std::string>{"Evil", ""});
    CHECK(f.library.size() == 2);
    const Toast toast = LibraryService::deleteToast(summary);
    CHECK(toast.type == ToastType::Error);
    CHECK(toast.message == "Unable to delete the selected sheets. Evil");
    CHECK(LibraryService::deleteConfirmation(3) ==
          "Delete 3 selected sheets? This cannot be undone.");
    CHECK(LibraryService::deleteConfirmation(1) ==
          "Delete 1 selected sheet? This cannot be undone.");
}

TEST_CASE("store imports reject duplicates and encrypted sheets") {
    Fixture f;
    const OrderedJson sheet = OrderedJson::parse(
        R"({"name":"Store","author":"A","transcribedBy":"T","isComposed":true,"bpm":100,"bitsPerPage":15,"pitchLevel":0,"isEncrypted":false,"songNotes":[{"time":0,"key":"1Key3"}],"hasHoldNotes":false})");
    const StoreSource source{"sky-sheet-store", "11111111-1111-4111-8111-111111111111",
                             "https://skysheet.store/sheets/x"};

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
    f.library.importStoreSheet(encrypted, StoreSource{"sky-sheet-store", "other", ""},
                               [&](Result<void> r) { result = r; });
    CHECK(result.error().userMessage == "Sheet has been encrypted!");
}

TEST_CASE("editor saves update the row and the payload") {
    Fixture f;
    const auto a = f.dir.path() / "in" / "a.txt";
    writeFile(a, "240\nA1");
    f.importAndWait({a});
    REQUIRE(f.library.updateEntryField(0, "bpm", 90).ok());
    CHECK(f.library.entry(0)->bpm() == 90.0);

    CHECK_FALSE(f.library.entry(0)->hasHoldNotes());
    auto edited = payloadFromSongNotes({SongNote{0, "1Key1", 300}});
    REQUIRE(edited);
    REQUIRE(f.library.saveEditedNotes(0, edited.value(), true).ok());
    const std::string text = readTextFile(f.dir.path() / f.library.entry(0)->payloadFile()).value();
    CHECK(
        text ==
        R"({"version":2,"keyMap":{"0":["u"],"1000":[]},"songNotes":[{"time":0,"key":"1Key1","hold":300}]})");
    CHECK(f.library.entry(0)->hasHoldNotes());
}

TEST_CASE("concurrent imports colliding on candidate payload names never overwrite each other") {
    ensureCoreApp();
    TempDir dir;
    std::atomic<int> saltCalls{0};
    StorageFileOps ops = StorageFileOps::native();
    ops.nextSalt = [&saltCalls]() -> std::int64_t {
        const int call = saltCalls.fetch_add(1);
        return call < 2 ? 42 : 42 + call;
    };

    PlaylistService playlists(dir.path(), ops);
    LibraryService library(dir.path(), playlists, ops);
    playlists.load();
    library.load();

    const auto a = dir.path() / "in" / "SameNameA.txt";
    const auto b = dir.path() / "in" / "SameNameB.txt";
    writeFile(a, "120\nA1");
    writeFile(b, "120\nB2");

    int finishedCount = 0;
    QObject::connect(&library, &LibraryService::importFinished, [&](const ImportSummary &s) {
        CHECK(s.succeeded == 1);
        ++finishedCount;
    });

    library.importFiles({a});
    library.importFiles({b});
    REQUIRE(waitUntil([&] { return finishedCount == 2; }));
    REQUIRE(library.size() == 2);
    const std::string key1 = library.entry(0)->payloadFile();
    const std::string key2 = library.entry(1)->payloadFile();
    CHECK(key1 != key2);
    CHECK(std::filesystem::exists(dir.path() / key1));
    CHECK(std::filesystem::exists(dir.path() / key2));

    LibraryService reloaded(dir.path(), playlists);
    reloaded.load();
    REQUIRE(reloaded.size() == 2);
    CHECK(reloaded.payload(0).ok());
    CHECK(reloaded.payload(1).ok());
}

TEST_CASE("imports roll back created payloads and keep memory clean when listSheet write fails") {
    ensureCoreApp();
    TempDir dir;
    bool failListWrite = false;
    StorageFileOps ops = StorageFileOps::native();
    ops.atomicWrite = [&failListWrite](const std::filesystem::path &file,
                                       std::string_view bytes) -> Result<void> {
        if (failListWrite && file.filename() == "listSheet.json") {
            return Error{"WRITE_FAILED", "Disk full", "injected listSheet.json failure"};
        }
        return StorageFileOps::native().atomicWrite(file, bytes);
    };

    PlaylistService playlists(dir.path(), ops);
    LibraryService library(dir.path(), playlists, ops);
    playlists.load();
    library.load();

    const auto source = dir.path() / "in" / "FailList.txt";
    writeFile(source, "120\nA1 B2");
    failListWrite = true;

    bool finished = false;
    ImportSummary summary;
    QObject::connect(&library, &LibraryService::importFinished, [&](const ImportSummary &s) {
        summary = s;
        finished = true;
    });
    library.importFiles({source});
    REQUIRE(waitUntil([&] { return finished; }));
    CHECK(summary.succeeded == 0);
    CHECK_FALSE(summary.failures.empty());
    CHECK(LibraryService::importToast(summary).type == ToastType::Error);
    CHECK(library.size() == 0);

    // No orphan payload left behind from the failed file import.
    int jsonFiles = 0;
    for (const auto &entry : std::filesystem::directory_iterator(dir.path())) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            ++jsonFiles;
        }
    }
    CHECK(jsonFiles == 1);  // only initial empty listSheet.json

    // Store import failure when listSheet write fails.
    const OrderedJson sheet = OrderedJson::parse(
        R"({"name":"StoreFail","author":"A","transcribedBy":"T","isComposed":true,"bpm":100,"bitsPerPage":15,"pitchLevel":0,"isEncrypted":false,"songNotes":[{"time":0,"key":"1Key3"}],"hasHoldNotes":false})");
    bool storeDone = false;
    Result<void> storeResult = Result<void>();
    library.importStoreSheet(sheet, StoreSource{"sky-sheet-store", "id-fail", ""},
                             [&](Result<void> r) {
                                 storeResult = r;
                                 storeDone = true;
                             });
    REQUIRE(waitUntil([&] { return storeDone; }));
    CHECK_FALSE(storeResult.ok());
    CHECK(library.size() == 0);

    // Legacy row import failure when listSheet write fails rolls back copied payloads.
    const auto legacyDir = dir.path() / "legacy";
    writeFile(legacyDir / "leg.json", R"({"version":2,"keyMap":{},"songNotes":[]})");
    OrderedJson legacyRow = OrderedJson::object();
    legacyRow["name"] = "LegacyFail";
    legacyRow["keyMap"] = "leg.json";
    bool legacyDone = false;
    LegacyImportSummary legacySummary;
    library.importLegacyRows(legacyDir, {legacyRow}, 0, [&](LegacyImportSummary s) {
        legacySummary = std::move(s);
        legacyDone = true;
    });
    REQUIRE(waitUntil([&] { return legacyDone; }));
    CHECK_FALSE(legacySummary.indexCommitted);
    CHECK(legacySummary.imported == 0);
    CHECK(legacySummary.failed == 1);
    CHECK(legacySummary.payloadMap.empty());
    CHECK_FALSE(std::filesystem::exists(dir.path() / "leg.json"));
    CHECK(library.size() == 0);

    LibraryService reloaded(dir.path(), playlists);
    reloaded.load();
    CHECK(reloaded.size() == 0);
}

TEST_CASE("entry and note edits preserve previous persisted state when index write fails") {
    ensureCoreApp();
    TempDir dir;
    bool failListWrite = false;
    bool failPayloadWrite = false;
    StorageFileOps ops = StorageFileOps::native();
    ops.atomicWrite = [&](const std::filesystem::path &file,
                          std::string_view bytes) -> Result<void> {
        if (failListWrite && file.filename() == "listSheet.json") {
            return Error{"WRITE_FAILED", "Disk full", "injected listSheet.json failure"};
        }
        if (failPayloadWrite && file.filename() != "listSheet.json") {
            return Error{"WRITE_FAILED", "Payload locked", "injected payload failure"};
        }
        return StorageFileOps::native().atomicWrite(file, bytes);
    };

    PlaylistService playlists(dir.path(), ops);
    LibraryService library(dir.path(), playlists, ops);
    playlists.load();
    library.load();

    const auto src = dir.path() / "in" / "EditSafe.txt";
    writeFile(src, "120\nA1");
    bool imported = false;
    QObject::connect(&library, &LibraryService::importFinished,
                     [&](const ImportSummary &) { imported = true; });
    library.importFiles({src});
    REQUIRE(waitUntil([&] { return imported; }));
    REQUIRE(library.size() == 1);
    const std::string originalKey = library.entry(0)->payloadFile();
    const std::string originalPayloadText = readTextFile(dir.path() / originalKey).value();

    // Entry edit fails on index write -> memory and disk keep bpm 120.
    failListWrite = true;
    CHECK_FALSE(library.updateEntryField(0, "bpm", 180).ok());
    CHECK(library.entry(0)->bpm() == 120.0);

    // Note edit changing hasHoldNotes fails on index write -> old payload & index intact, new
    // payload cleaned up.
    auto edited = payloadFromSongNotes({SongNote{0, "1Key2", 300}});
    REQUIRE(edited.ok());
    CHECK_FALSE(library.saveEditedNotes(0, edited.value(), true).ok());
    CHECK(library.entry(0)->payloadFile() == originalKey);
    CHECK_FALSE(library.entry(0)->hasHoldNotes());
    CHECK(readTextFile(dir.path() / originalKey).value() == originalPayloadText);

    // Succeeding metadata-changing note edit switches to a new payload filename and removes the old
    // one.
    failListWrite = false;
    REQUIRE(library.saveEditedNotes(0, edited.value(), true).ok());
    const std::string newKey = library.entry(0)->payloadFile();
    CHECK(newKey != originalKey);
    CHECK_FALSE(std::filesystem::exists(dir.path() / originalKey));
    CHECK(std::filesystem::exists(dir.path() / newKey));

    // Failing payload write when metadata does not change keeps old payload intact.
    failPayloadWrite = true;
    auto edited2 = payloadFromSongNotes({SongNote{0, "1Key4", 200}});
    REQUIRE(edited2.ok());
    CHECK_FALSE(library.saveEditedNotes(0, edited2.value(), true).ok());

    LibraryService reloaded(dir.path(), playlists);
    reloaded.load();
    REQUIRE(reloaded.size() == 1);
    CHECK(reloaded.entry(0)->payloadFile() == newKey);
    CHECK(reloaded.entry(0)->hasHoldNotes());
    REQUIRE(reloaded.payload(0).value()->songNotes.has_value());
    CHECK((*reloaded.payload(0).value()->songNotes)[0].key == "1Key2");
}

TEST_CASE("deletion honors index-then-playlist-then-payload write order under injected failures") {
    ensureCoreApp();
    TempDir dir;
    bool failListWrite = false;
    bool failPlaylistWrite = false;
    bool failRemove = false;
    StorageFileOps ops = StorageFileOps::native();
    ops.atomicWrite = [&](const std::filesystem::path &file,
                          std::string_view bytes) -> Result<void> {
        if (failListWrite && file.filename() == "listSheet.json") {
            return Error{"WRITE_FAILED", "list error", "listSheet.json write blocked"};
        }
        if (failPlaylistWrite && file.filename() == "playlists.json") {
            return Error{"WRITE_FAILED", "playlist error", "playlists.json write blocked"};
        }
        return StorageFileOps::native().atomicWrite(file, bytes);
    };
    ops.removeFile = [&](const std::filesystem::path &file) -> Result<void> {
        if (failRemove) {
            return Error{"DELETE_FAILED", "file locked", "injected remove failure"};
        }
        return StorageFileOps::native().removeFile(file);
    };

    PlaylistService playlists(dir.path(), ops);
    LibraryService library(dir.path(), playlists, ops);
    playlists.load();
    library.load();

    const auto a = dir.path() / "in" / "A.txt";
    const auto b = dir.path() / "in" / "B.txt";
    const auto c = dir.path() / "in" / "C.txt";
    writeFile(a, "120\nA1");
    writeFile(b, "120\nB1");
    writeFile(c, "120\nC1");
    int batches = 0;
    QObject::connect(&library, &LibraryService::importFinished,
                     [&](const ImportSummary &) { ++batches; });
    library.importFiles({a, b, c});
    REQUIRE(waitUntil([&] { return batches == 1; }));
    REQUIRE(library.size() == 3);
    const std::string keyA = library.entry(0)->payloadFile();
    const std::string keyB = library.entry(1)->payloadFile();
    const std::string keyC = library.entry(2)->payloadFile();
    const std::string pid = playlists.create("Mix").value().id;
    REQUIRE(playlists.addSheets(pid, {keyA, keyB, keyC}).ok());

    // 1. Index write fails -> nothing changes anywhere.
    failListWrite = true;
    const DeleteSummary failIdx = library.deleteSheet(0);
    CHECK_FALSE(failIdx.indexCommitted);
    CHECK_FALSE(failIdx.playlistUpdated);
    CHECK_FALSE(failIdx.payloadCleanupCompleted);
    CHECK_FALSE(failIdx.ok());
    CHECK(library.size() == 3);
    CHECK(std::filesystem::exists(dir.path() / keyA));
    CHECK(playlists.find(pid)->sheetKeys.size() == 3);
    CHECK(LibraryService::deleteToast(failIdx).type == ToastType::Error);
    failListWrite = false;

    // 2. Playlist write fails after index commit -> library updated, playlist restored, payload
    // retained!
    failPlaylistWrite = true;
    const DeleteSummary failPl = library.deleteSheet(0);
    CHECK(failPl.indexCommitted);
    CHECK_FALSE(failPl.playlistUpdated);
    CHECK_FALSE(failPl.payloadCleanupCompleted);
    CHECK_FALSE(failPl.ok());
    CHECK(library.size() == 2);
    CHECK(std::filesystem::exists(dir.path() /
                                  keyA));  // retained because playlist still references it
    CHECK(playlists.find(pid)->sheetKeys == std::vector<std::string>{keyA, keyB, keyC});
    CHECK(LibraryService::deleteToast(failPl).type == ToastType::Warning);

    // Reloading detects stale playlist reference without silently rewriting playlists.json or
    // deleting keyA.
    PlaylistService reloadedPlaylists(dir.path());
    LibraryService reloadedLibrary(dir.path(), reloadedPlaylists);
    reloadedPlaylists.load();
    reloadedLibrary.load();
    CHECK(reloadedLibrary.size() == 2);
    const auto stale = reloadedLibrary.stalePlaylistReferences();
    REQUIRE(stale.size() == 1);
    CHECK(stale[0].sheetKey == keyA);
    CHECK(reloadedPlaylists.find(pid)->sheetKeys.size() == 3);
    CHECK(std::filesystem::exists(dir.path() / keyA));
    failPlaylistWrite = false;

    // 3. Payload cleanup fails after index + playlist commit -> partial warning, library & playlist
    // committed.
    failRemove = true;
    const DeleteSummary failClean = library.deleteSheets({keyB});
    CHECK(failClean.indexCommitted);
    CHECK(failClean.playlistUpdated);
    CHECK_FALSE(failClean.payloadCleanupCompleted);
    CHECK(failClean.cleanupFailedKeys == std::vector<std::string>{keyB});
    CHECK(LibraryService::deleteToast(failClean).type == ToastType::Warning);
    CHECK(library.size() == 1);
    CHECK(playlists.find(pid)->sheetKeys == std::vector<std::string>{keyA, keyC});
    CHECK(std::filesystem::exists(dir.path() / keyB));
    failRemove = false;

    // 4. Full success.
    const DeleteSummary fullOk = library.deleteSheet(0);
    CHECK(fullOk.ok());
    CHECK(fullOk.indexCommitted);
    CHECK(fullOk.playlistUpdated);
    CHECK(fullOk.payloadCleanupCompleted);
    CHECK_FALSE(std::filesystem::exists(dir.path() / keyC));
}
