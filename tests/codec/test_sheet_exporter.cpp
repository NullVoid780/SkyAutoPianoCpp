#include <catch2/catch_test_macros.hpp>

#include "codec/sheet_exporter.hpp"

using namespace sky;

TEST_CASE("picks the layout from the extension")
{
    CHECK(exportLayoutForPath("C:/songs/a.TXT") == ExportLayout::Compact);
    CHECK(exportLayoutForPath("a.json") == ExportLayout::Pretty);
    CHECK(exportLayoutForPath("a.skysheet") == ExportLayout::Pretty);
    CHECK(exportLayoutForPath("noextension") == ExportLayout::Pretty);
}

TEST_CASE("rebuilds song notes from the keymap when the payload has none")
{
    const KeyMap keyMap({{0.0, {"y", "/"}}, {500.0, {"h", "q"}}, {1000.0, {}}});
    CHECK(songNotesFromKeyMap(keyMap) ==
          std::vector<SongNote>{{0, "1Key0", 0}, {0, "1Key14", 0}, {500, "1Key5", 0}});
}

TEST_CASE("exports the original single-object array shape")
{
    const SheetEntry entry(OrderedJson::parse(
        R"({"name":"Song","author":"","bpm":90,"bitsPerPage":15,"pitchLevel":0,"isComposed":true,"keyMap":"x.json"})"));
    const KeyMap keyMap({{0.0, {"y"}}, {1000.0, {}}});
    const OrderedJson document = buildExportDocument(entry, {}, keyMap);
    CHECK(serializeExport(document, ExportLayout::Compact) ==
          R"([{"name":"Song","author":"Unknown","transcribedBy":"Unknown","isComposed":true,"bpm":90,"bitsPerPage":15,"pitchLevel":0,"isEncrypted":false,"songNotes":[{"time":0,"key":"1Key0"}]}])");
}

TEST_CASE("pretty export uses four-space indentation")
{
    const SheetEntry entry(OrderedJson::parse(R"({"name":"S"})"));
    const std::vector<SongNote> notes{{0, "1Key0", 250}};
    const std::string text = serializeExport(buildExportDocument(entry, notes, KeyMap()), ExportLayout::Pretty);
    CHECK(text.find("\n        \"name\": \"S\"") != std::string::npos);
    CHECK(text.find("\"hold\": 250") != std::string::npos);
    CHECK(text.find("bitsPerPage") == std::string::npos);
}
