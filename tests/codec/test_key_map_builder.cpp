#include <catch2/catch_test_macros.hpp>

#include "codec/key_map_builder.hpp"
#include "codec/playback_note_builder.hpp"

using namespace sky;

TEST_CASE("builds a keymap with a zero entry and a terminal marker")
{
    auto keyMap = buildKeyMap({{480, "y", 0}, {480, "u", 0}, {480, "y", 0}, {960, "i", 1500}});
    REQUIRE(keyMap.ok());
    CHECK(dumpJson(keyMap.value().toJson()) == R"({"0":[],"480":["y","u"],"960":["i"],"3000":[]})");
    CHECK(keyMap.value().lastTime() == 3000.0);
}

TEST_CASE("the terminal marker rounds up to the next whole second")
{
    auto exact = buildKeyMap({{0, "y", 0}, {2000, "u", 0}});
    REQUIRE(exact.ok());
    CHECK(exact.value().lastTime() == 3000.0);
}

TEST_CASE("an empty note list has no playable notes")
{
    auto keyMap = buildKeyMap({});
    REQUIRE_FALSE(keyMap.ok());
    CHECK(keyMap.error().code == "NO_NOTES");
}

TEST_CASE("normalises stored keymaps")
{
    const auto raw = OrderedJson::parse(R"({"1000":[],"abc":["y"],"-5":["y"],"0":["y","y"," u ","q",3]})");
    auto keyMap = normalizeStoredKeyMap(raw);
    REQUIRE(keyMap.ok());
    REQUIRE(keyMap.value().size() == 2);
    CHECK(keyMap.value()[0] == KeyMapEntry{0.0, {"y", "u"}});
    CHECK(keyMap.value()[1] == KeyMapEntry{1000.0, {}});
}

TEST_CASE("serialises array-index keys before fractional ones")
{
    const KeyMap keyMap({{0.0, {}}, {1.5, {"y"}}, {2.0, {}}});
    CHECK(dumpJson(keyMap.toJson()) == R"({"0":[],"2":[],"1.5":["y"]})");
}

TEST_CASE("filters a keymap from a seek position")
{
    const KeyMap keyMap({{0.0, {"y"}}, {500.0, {"u"}}, {1000.0, {}}});
    const KeyMap filtered = keyMap.filteredFrom(500.0);
    REQUIRE(filtered.size() == 2);
    CHECK(filtered[0].time == 500.0);
}

TEST_CASE("playback notes reject keys beyond bitsPerPage")
{
    auto notes = buildPlaybackNotes({{0, "1Key9", 0}}, 8);
    REQUIRE_FALSE(notes.ok());
    CHECK(notes.error().code == "INVALID_NOTE_KEY");
    CHECK(notes.error().userMessage == "Unsupported playback key “" "1Key9" "”.");
}
