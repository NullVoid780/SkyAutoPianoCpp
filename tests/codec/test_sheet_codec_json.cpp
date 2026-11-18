#include <catch2/catch_test_macros.hpp>

#include <string>

#include "codec/sheet_codec.hpp"
#include "test_helpers.hpp"

using namespace sky;

namespace {

Result<ParsedSheet> parseText(std::string_view text, std::string_view fileName = "sheet.json")
{
    const auto bytes = bytesOf(text);
    return parseSheet(bytes, fileName);
}

std::string fixture(std::string_view notes, std::string_view extra = "")
{
    return std::string(R"([{"name":"Fixture","author":"Tester","bpm":120,"pitchLevel":0,"isEncrypted":false)") +
           std::string(extra) + R"(,"songNotes":)" + std::string(notes) + "}]";
}

bool hasWarning(const ParsedSheet &parsed, std::string_view code)
{
    for (const SheetWarning &warning : parsed.warnings) {
        if (warning.code == code) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("imports Sky Music Studio JSON with unprefixed keys")
{
    auto result = parseText(
        R"([{"name":"Rubia","author":"Sky Music Studio","bpm":68,"pitchLevel":0,"isEncrypted":false,"songNotes":[{"time":0,"key":"Key5"},{"time":882,"key":"Key2"}]}])",
        "Rubia.json");
    REQUIRE(result.ok());
    const ParsedSheet &parsed = result.value();

    REQUIRE(parsed.sheet.songNotes.size() == 2);
    CHECK(parsed.sheet.songNotes[0] == SongNote{0, "1Key5", 0});
    CHECK(parsed.sheet.songNotes[1] == SongNote{882, "1Key2", 0});
    CHECK(dumpJson(parsed.storage.keyMap.toJson()) == R"({"0":["h"],"882":["i"],"1000":[]})");
    CHECK(parsed.preview.format == SheetFormat::Json);
    CHECK(parsed.preview.name == "Rubia");
    CHECK(parsed.preview.noteCount == 2);
    CHECK(parsed.preview.durationMs == 882);
    REQUIRE(hasWarning(parsed, "UNPREFIXED_KEYS_NORMALIZED"));
    CHECK(parsed.warnings.back().count == 2);
}

TEST_CASE("the stored payload is byte-identical to the Electron build")
{
    auto result = parseText(
        R"([{"name":"Rubia","bpm":68,"songNotes":[{"time":882,"key":"1Key2"},{"time":0,"key":"1Key5","hold":320}]}])");
    REQUIRE(result.ok());
    CHECK(dumpJson(result.value().storage.toJson()) ==
          R"({"version":2,"keyMap":{"0":["h"],"882":["i"],"1000":[]},"songNotes":[{"time":0,"key":"1Key5","hold":320},{"time":882,"key":"1Key2"}]})");
}

TEST_CASE("matches the payload produced by the Electron codec under Node")
{
    auto result = parseText(
        R"([{"name":"R","bpm":68,"songNotes":[{"time":882,"key":"1Key2"},{"time":0,"key":"1Key5","hold":320},{"time":0,"key":"1Key9"},{"time":0,"key":"1Key12"},{"time":0,"key":"1Key13"}]}])",
        "R.json");
    REQUIRE(result.ok());
    CHECK(dumpJson(result.value().storage.toJson()) ==
          R"({"version":2,"keyMap":{"0":[",",";",".","h"],"882":["i"],"1000":[]},"songNotes":[{"time":0,"key":"1Key12"},{"time":0,"key":"1Key13"},{"time":0,"key":"1Key5","hold":320},{"time":0,"key":"1Key9"},{"time":882,"key":"1Key2"}]})");
}

TEST_CASE("imports UTF-8 BOM and UTF-16 LE JSON")
{
    const std::string source = fixture(R"([{"time":0,"key":"1Key0"}])");

    auto bom = bytesOf("\xEF\xBB\xBF" + source);
    auto utf8 = parseSheet(bom, "encoding.json");
    REQUIRE(utf8.ok());
    CHECK(utf8.value().preview.name == "Fixture");

    auto wide = utf16le(source, true);
    auto utf16 = parseSheet(wide, "encoding.json");
    REQUIRE(utf16.ok());
    CHECK(utf16.value().preview.noteCount == 1);

    auto bare = utf16le(source, false);
    auto detected = parseSheet(bare, "encoding.json");
    REQUIRE(detected.ok());
    CHECK(detected.value().preview.name == "Fixture");
}

TEST_CASE("accepts relaxed JSON and call wrappers")
{
    auto relaxed = parseText(R"([{name:'Relaxed', bpm: 100, songNotes:[{time:0,key:'1Key3',},],}])");
    REQUIRE(relaxed.ok());
    CHECK(relaxed.value().sheet.name == "Relaxed");
    CHECK(relaxed.value().sheet.bpm == 100.0);

    auto wrapped = parseText(R"(([{"name":"Wrapped","songNotes":[{"time":0,"key":"1Key0"}]}]);)");
    REQUIRE(wrapped.ok());
    CHECK(wrapped.value().sheet.name == "Wrapped");
}

TEST_CASE("orders chord keys with ICU collation")
{
    auto result = parseText(fixture(
        R"([{"time":0,"key":"1Key0"},{"time":0,"key":"1Key9"},{"time":0,"key":"1Key12"},{"time":0,"key":"1Key5"},{"time":0,"key":"1Key13"},{"time":0,"key":"1Key14"}])"));
    REQUIRE(result.ok());
    const auto &first = result.value().storage.keyMap[0];
    CHECK(first.keys == std::vector<std::string>{",", ";", ".", "/", "h", "y"});
}

TEST_CASE("reports the exact invalid note")
{
    auto outOfRange = parseText(fixture(R"([{"time":250,"key":"Key99"}])"), "broken.json");
    REQUIRE_FALSE(outOfRange.ok());
    CHECK(outOfRange.error().code == "KEY_OUT_OF_RANGE");
    CHECK(outOfRange.error().userMessage == "Note 1 uses key 99, but supported keys are 0–14.");

    auto badKey = parseText(fixture(R"([{"time":0,"key":"1Key0"},{"time":10,"key":"abc"}])"));
    REQUIRE_FALSE(badKey.ok());
    CHECK(badKey.error().code == "INVALID_NOTE_KEY");
    CHECK(badKey.error().userMessage == "Note 2 uses unsupported key “" "abc" "”.");

    auto emptyKey = parseText(fixture(R"([{"time":0}])"));
    REQUIRE_FALSE(emptyKey.ok());
    CHECK(emptyKey.error().userMessage == "Note 1 uses unsupported key “(empty)”.");

    auto badTime = parseText(fixture(R"([{"time":-1,"key":"1Key0"}])"));
    REQUIRE_FALSE(badTime.ok());
    CHECK(badTime.error().code == "INVALID_NOTE_TIME");
    CHECK(badTime.error().userMessage == "Note 1 has an invalid time value.");

    auto tooLate = parseText(fixture(R"([{"time":900001,"key":"1Key0"}])"));
    REQUIRE_FALSE(tooLate.ok());
    CHECK(tooLate.error().code == "INVALID_NOTE_TIME");
}

TEST_CASE("rejects sheets without notes or with too many")
{
    auto empty = parseText(fixture("[]"));
    REQUIRE_FALSE(empty.ok());
    CHECK(empty.error().code == "NO_NOTES");
    CHECK(empty.error().userMessage == "The sheet does not contain any playable notes.");

    std::string many = "[";
    for (int i = 0; i < 20001; ++i) {
        many += (i ? "," : "") + std::string(R"({"time":0,"key":"1Key0"})");
    }
    many += "]";
    auto tooMany = parseText(fixture(many));
    REQUIRE_FALSE(tooMany.ok());
    CHECK(tooMany.error().code == "TOO_MANY_NOTES");
    CHECK(tooMany.error().userMessage == "The sheet contains 20001 notes; the limit is 20000.");
}

TEST_CASE("structural JSON failures use the original codes and messages")
{
    auto blank = parseText("  \n\t ");
    REQUIRE_FALSE(blank.ok());
    CHECK(blank.error().code == "EMPTY_FILE");
    CHECK(blank.error().userMessage == "The selected sheet file is empty.");

    auto emptyArray = parseText("[]");
    REQUIRE_FALSE(emptyArray.ok());
    CHECK(emptyArray.error().code == "INVALID_JSON");
    CHECK(emptyArray.error().userMessage == "Invalid sheet JSON: The sheet JSON array is empty.");

    auto scalar = parseText("5");
    REQUIRE_FALSE(scalar.ok());
    CHECK(scalar.error().code == "INVALID_JSON");
    CHECK(scalar.error().userMessage ==
          "Invalid sheet JSON: The sheet JSON must be an object or an array containing one object.");

    auto notObject = parseText("[5]");
    REQUIRE_FALSE(notObject.ok());
    CHECK(notObject.error().code == "INVALID_SHEET");
    CHECK(notObject.error().userMessage == "The sheet must contain one song object.");

    auto garbage = parseText("{not json at all");
    REQUIRE_FALSE(garbage.ok());
    CHECK(garbage.error().code == "INVALID_JSON");
    CHECK(garbage.error().userMessage.rfind("Invalid sheet JSON: ", 0) == 0);
}

TEST_CASE("defaults invalid metadata with warnings")
{
    auto result = parseText(R"({"songNotes":[{"time":0,"key":"1Key0"}],"bpm":"fast","pitchLevel":40})", "Song.TXT");
    REQUIRE(result.ok());
    const ParsedSheet &parsed = result.value();
    CHECK(parsed.sheet.bpm == 120.0);
    CHECK(parsed.sheet.pitchLevel == 0.0);
    CHECK(parsed.sheet.name == "Song");
    CHECK(parsed.sheet.author == "Unknown");
    CHECK(parsed.sheet.transcribedBy == "Unknown");
    CHECK(parsed.sheet.isComposed);
    CHECK(hasWarning(parsed, "BPM_DEFAULTED"));
    CHECK(hasWarning(parsed, "PITCH_DEFAULTED"));
}

TEST_CASE("raises bitsPerPage to fit the highest key")
{
    auto raised = parseText(fixture(R"([{"time":0,"key":"1Key10"}])", R"(,"bitsPerPage":8)"));
    REQUIRE(raised.ok());
    CHECK(raised.value().sheet.bitsPerPage == 11);

    auto kept = parseText(fixture(R"([{"time":0,"key":"1Key3"}])", R"(,"bitsPerPage":8)"));
    REQUIRE(kept.ok());
    CHECK(kept.value().sheet.bitsPerPage == 8);
}

TEST_CASE("isComposed is true unless explicitly false")
{
    auto composed = parseText(fixture(R"([{"time":0,"key":"1Key0"}])", R"(,"isComposed":false)"));
    REQUIRE(composed.ok());
    CHECK_FALSE(composed.value().sheet.isComposed);
}

TEST_CASE("keeps the longest hold for duplicate playback notes")
{
    auto result = parseText(fixture(
        R"([{"time":0,"key":"1Key0","hold":100},{"time":0,"key":"Key0","hold":300},{"time":0,"key":"1Key0"}])"));
    REQUIRE(result.ok());
    CHECK(result.value().preview.durationMs == 300);
    CHECK(result.value().preview.hasHoldNotes);
    CHECK(dumpJson(result.value().storage.keyMap.toJson()) == R"({"0":["y"],"1000":[]})");
}

TEST_CASE("loads stored payloads for the player and editor")
{
    const auto payload = OrderedJson::parse(R"({"version":2,"keyMap":{"0":["y","y"],"1000":[]},"songNotes":[{"time":0,"key":"Key0","hold":400}]})");
    auto loaded = loadStoredSheet(payload, 16);
    REQUIRE(loaded.ok());
    const StoredSheet &stored = loaded.value();
    CHECK(dumpJson(stored.keyMap.toJson()) == R"({"0":["y"],"1000":[]})");
    REQUIRE(stored.songNotes.has_value());
    CHECK(*stored.songNotes == std::vector<SongNote>{{0, "1Key0", 400}});
    REQUIRE(stored.playbackNotes.has_value());
    CHECK(*stored.playbackNotes == std::vector<PlaybackNote>{{0, "y", 400}});
    CHECK(stored.hasHoldNotes);
}

TEST_CASE("loads version 1 payloads where the object is the keymap")
{
    const auto payload = OrderedJson::parse(R"({"0":["y"],"500":["u","i"],"2000":[]})");
    auto loaded = loadStoredSheet(payload);
    REQUIRE(loaded.ok());
    CHECK(loaded.value().keyMap.size() == 3);
    CHECK_FALSE(loaded.value().songNotes.has_value());
    CHECK_FALSE(loaded.value().hasHoldNotes);
    CHECK(loaded.value().bitsPerPage == 16);
}

TEST_CASE("rejects invalid stored payloads")
{
    auto array = loadStoredSheet(OrderedJson::array());
    REQUIRE_FALSE(array.ok());
    CHECK(array.error().code == "INVALID_STORED_PAYLOAD");
    CHECK(array.error().userMessage == "The stored sheet payload is invalid.");

    auto badKeyMap = loadStoredSheet(OrderedJson::parse(R"({"keyMap":[]})"));
    REQUIRE_FALSE(badKeyMap.ok());
    CHECK(badKeyMap.error().code == "INVALID_KEYMAP");
    CHECK(badKeyMap.error().userMessage == "The stored sheet keymap is invalid.");
}
