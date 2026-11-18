#include <catch2/catch_test_macros.hpp>

#include "codec/midi_file.hpp"
#include "codec/sheet_codec.hpp"
#include "test_helpers.hpp"

using namespace sky;

namespace {

std::vector<std::byte> middleC()
{
    return bytesOf({
        0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x60,
        0x4d, 0x54, 0x72, 0x6b, 0x00, 0x00, 0x00, 0x13,
        0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20,
        0x00, 0x90, 0x3c, 0x40,
        0x60, 0x80, 0x3c, 0x40,
        0x00, 0xff, 0x2f, 0x00,
    });
}

}  // namespace

TEST_CASE("imports a standard MIDI file")
{
    const auto bytes = middleC();
    auto result = parseSheet(bytes, "middle-c.mid");
    REQUIRE(result.ok());
    const ParsedSheet &parsed = result.value();
    CHECK(parsed.preview.format == SheetFormat::Midi);
    CHECK(parsed.preview.name == "middle-c");
    CHECK(parsed.preview.noteCount == 1);
    CHECK(parsed.sheet.bpm == 120.0);
    CHECK(parsed.sheet.songNotes == std::vector<SongNote>{{0, "1Key0", 0}});
}

TEST_CASE("MIDI reader exposes tempo, running status and timing")
{
    const auto bytes = bytesOf({
        0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x01, 0xe0,
        0x4d, 0x54, 0x72, 0x6b, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xff, 0x03, 0x04, 'S', 'o', 'n', 'g',
        0x00, 0xff, 0x02, 0x05, 'A', ',', ' ', 'B', 'x',
        0x00, 0x90, 0x3e, 0x40,
        0x83, 0x60, 0x40, 0x40,
        0x00, 0xff, 0x2f, 0x00,
    });
    auto midi = readMidiFile(bytes);
    REQUIRE(midi.ok());
    CHECK(midi.value().timeDivision == 480);
    REQUIRE(midi.value().tracks.size() == 1);

    auto result = parseSheet(bytes, "named.midi");
    REQUIRE(result.ok());
    const Sheet &sheet = result.value().sheet;
    CHECK(sheet.name == "Song");
    CHECK(sheet.author == "A");
    CHECK(sheet.transcribedBy == "Bx");
    CHECK(sheet.songNotes == std::vector<SongNote>{{0, "1Key1", 0}, {500, "1Key2", 0}});
}

TEST_CASE("unmapped MIDI notes snap to the nearest Sky key")
{
    const auto bytes = bytesOf({
        0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x60,
        0x4d, 0x54, 0x72, 0x6b, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x90, 0x3d, 0x40,
        0x00, 0x90, 0x10, 0x40,
        0x00, 0x90, 0x7f, 0x40,
        0x00, 0xff, 0x2f, 0x00,
    });
    auto result = parseSheet(bytes, "snap.mid");
    REQUIRE(result.ok());
    CHECK(result.value().sheet.songNotes ==
          std::vector<SongNote>{{0, "1Key0", 0}, {0, "1Key0", 0}, {0, "1Key14", 0}});
}

TEST_CASE("MIDI failures use the original codes")
{
    auto empty = parseSheet({}, "empty.mid");
    REQUIRE_FALSE(empty.ok());
    CHECK(empty.error().code == "EMPTY_MIDI");
    CHECK(empty.error().userMessage == "The selected MIDI file is empty.");

    const auto notMidi = bytesOf("not a midi file");
    auto rejected = parseSheet(notMidi, "fake.mid");
    REQUIRE_FALSE(rejected.ok());
    CHECK(rejected.error().code == "INVALID_MIDI");
    CHECK(rejected.error().userMessage == "The MIDI file does not contain any tracks.");

    const auto noStatus = bytesOf({
        0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x60,
        0x4d, 0x54, 0x72, 0x6b, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x3c, 0x40,
    });
    auto malformed = parseSheet(noStatus, "broken.mid");
    REQUIRE_FALSE(malformed.ok());
    CHECK(malformed.error().code == "INVALID_MIDI");
    CHECK(malformed.error().userMessage.rfind("Invalid MIDI file: ", 0) == 0);

    const auto truncated = bytesOf({
        0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x60,
        0x4d, 0x54, 0x72, 0x6b, 0x00, 0x00, 0x00, 0x00,
        0x81,
    });
    auto cut = parseSheet(truncated, "cut.mid");
    REQUIRE_FALSE(cut.ok());
    CHECK(cut.error().userMessage == "Invalid MIDI file: Offset is outside the bounds of the DataView");
}
