#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "services/playback/timeline_builder.hpp"

using namespace sky;
using Catch::Matchers::WithinAbs;

namespace {

std::string identity(const std::string &key)
{
    return key;
}

}  // namespace

TEST_CASE("the keymap timeline staggers chords by 3 ms and presses for 25 ms")
{
    const KeyMap keyMap({{0.0, {"y", "u"}}, {1000.0, {"i"}}, {2000.0, {}}});
    const auto timeline = buildKeyMapTimeline(keyMap, {0.0, false, 1000.0}, identity);
    REQUIRE(timeline.size() == 6);
    CHECK(timeline[0] == TimelineEvent{0.0, "y", true});
    CHECK(timeline[1] == TimelineEvent{3.0, "u", true});
    CHECK(timeline[2] == TimelineEvent{25.0, "y", false});
    CHECK(timeline[3] == TimelineEvent{28.0, "u", false});
    CHECK_THAT(timeline[4].songTime, WithinAbs(1003.0, 1e-9));
    CHECK(timeline[4].key == "i");
    CHECK_THAT(timeline[5].songTime, WithinAbs(1028.0, 1e-9));
}

TEST_CASE("long press in the keymap timeline holds until the next step minus 35 ms")
{
    const KeyMap keyMap({{0.0, {"y"}}, {500.0, {"u"}}, {1000.0, {}}});
    const auto timeline = buildKeyMapTimeline(keyMap, {0.0, true, 2000.0}, identity);
    REQUIRE(timeline.size() == 4);
    CHECK_THAT(timeline[1].songTime, WithinAbs(500 * 1.003 - 35, 1e-9));
    CHECK_FALSE(timeline[1].press);
}

TEST_CASE("the last keymap step uses delayNext uncalibrated")
{
    const KeyMap keyMap({{0.0, {"y"}}});
    const auto timeline = buildKeyMapTimeline(keyMap, {0.0, true, 1000.0}, identity);
    REQUIRE(timeline.size() == 2);
    CHECK_THAT(timeline[1].songTime, WithinAbs(965.0, 1e-9));
}

TEST_CASE("steps before the seek position are skipped")
{
    const KeyMap keyMap({{0.0, {"y"}}, {30000.0, {"u"}}, {31000.0, {}}});
    const auto timeline = buildKeyMapTimeline(keyMap, {30000.0, false, 1000.0}, identity);
    REQUIRE(timeline.size() == 2);
    CHECK(timeline[0].key == "u");
    CHECK_THAT(timeline[0].songTime, WithinAbs(30090.0, 1e-9));
}

TEST_CASE("releases sort before presses at the same song time")
{
    std::vector<TimelineEvent> timeline{{10.0, "a", true}, {10.0, "b", false}, {5.0, "c", true}, {10.0, "d", false}};
    sortTimeline(timeline);
    CHECK(timeline[0].key == "c");
    CHECK(timeline[1].key == "b");
    CHECK(timeline[2].key == "d");
    CHECK(timeline[3].key == "a");
}

TEST_CASE("the note timeline honours holds and trims notes that started before the seek")
{
    const std::vector<PlaybackNote> notes{{0, "y", 3000}, {0, "u", 0}, {2000, "i", 0}};
    const auto timeline = buildNoteTimeline(notes, {1000.0, true, 1000.0}, identity);
    REQUIRE(timeline.size() == 4);
    CHECK(timeline[0].key == "y");
    CHECK_THAT(timeline[0].songTime, WithinAbs(1003.0, 1e-9));
    CHECK(timeline[1] == TimelineEvent{2000 * 1.003, "i", true});
    CHECK(timeline[2].key == "y");
    CHECK_FALSE(timeline[2].press);
    CHECK_THAT(timeline[2].songTime, WithinAbs(1003.0 + 2000 * 1.003 - 35, 1e-9));
    CHECK(timeline[3].key == "i");
}

TEST_CASE("note timeline without long press uses the minimum press and step durations")
{
    const std::vector<PlaybackNote> notes{{0, "y", 0}, {0, "u", 0}, {400, "i", 0}};
    const auto off = buildNoteTimeline(notes, {0.0, false, 1000.0}, identity);
    REQUIRE(off.size() == 6);
    CHECK(off[1] == TimelineEvent{3.0, "u", true});
    CHECK(off[2] == TimelineEvent{25.0, "y", false});

    const auto on = buildNoteTimeline(notes, {0.0, true, 1000.0}, identity);
    CHECK_THAT(on[2].songTime, WithinAbs(400 * 1.003 - 35, 1e-9));
}

TEST_CASE("notes with an empty resolved key are skipped")
{
    const std::vector<PlaybackNote> notes{{0, "y", 0}};
    const auto timeline = buildNoteTimeline(notes, {}, [](const std::string &) { return std::string(); });
    CHECK(timeline.empty());
}

TEST_CASE("prepared playback notes are trimmed and collation-sorted")
{
    const auto notes = preparePlaybackNotes({{10, "y", 0}, {10, " ,", 0}, {-1, "u", 0}, {5, "", 0}});
    REQUIRE(notes.size() == 2);
    CHECK(notes[0].key == ",");
    CHECK(notes[1].key == "y");
}
