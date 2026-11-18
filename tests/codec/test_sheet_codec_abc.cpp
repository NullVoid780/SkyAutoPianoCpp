#include <catch2/catch_test_macros.hpp>

#include "codec/abc_sheet_parser.hpp"
#include "codec/sheet_codec.hpp"
#include "test_helpers.hpp"

using namespace sky;

TEST_CASE("imports ABC text")
{
    const auto bytes = bytesOf("<DontCopyThisLine> 120 0 16 Author Transcriber\nA1 A2A3 .");
    auto result = parseSheet(bytes, "example.txt");
    REQUIRE(result.ok());
    const ParsedSheet &parsed = result.value();
    CHECK(parsed.preview.format == SheetFormat::Abc);
    CHECK(parsed.preview.name == "example");
    CHECK(parsed.preview.noteCount == 3);
    CHECK(parsed.sheet.author == "Author");
    CHECK(parsed.sheet.transcribedBy == "Transcriber");
    CHECK(parsed.sheet.songNotes ==
          std::vector<SongNote>{{0, "1Key0", 0}, {500, "1Key1", 0}, {500, "1Key2", 0}});
}

TEST_CASE("ABC dots and blank lines still advance time")
{
    const auto bytes = bytesOf("240 0 16\r\n. B1\r\n\r\nC5");
    auto result = parseSheet(bytes, "rests.txt");
    REQUIRE(result.ok());
    CHECK(result.value().sheet.songNotes ==
          std::vector<SongNote>{{250, "1Key5", 0}, {750, "1Key14", 0}});
    CHECK(result.value().sheet.bpm == 240.0);
}

TEST_CASE("ABC metadata falls back to defaults")
{
    const OrderedJson sheet = parseAbcSheet("0 x\nA1", "name");
    CHECK(sheet["bpm"] == 240);
    CHECK(sheet["pitchLevel"] == 0);
    CHECK(sheet["bitsPerPage"] == 16);
    CHECK(sheet["author"] == "Unknown");
    CHECK(sheet["transcribedBy"] == "Unknown");
}

TEST_CASE("ABC detection")
{
    CHECK(looksLikeAbc("<DontCopyThisLine> 120"));
    CHECK(looksLikeAbc("120 0 16\nA1 B2"));
    CHECK(looksLikeAbc("C5"));
    CHECK_FALSE(looksLikeAbc(R"([{"key":"A1x"}])"));
    CHECK_FALSE(looksLikeAbc("D1 E2"));
}

TEST_CASE("ABC file stems keep the case-sensitive basename quirk")
{
    const auto bytes = bytesOf("120\nA1");
    auto result = parseSheet(bytes, "Song.TXT");
    REQUIRE(result.ok());
    CHECK(result.value().sheet.name == "Song.TXT");
}
