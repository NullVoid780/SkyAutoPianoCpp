#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "codec/sheet_exporter.hpp"
#include "ui/editor_window/editor_document.hpp"

using namespace sky;

namespace {

const TimeGrid kGrid(120, 2);

EditorDocument sample()
{
    return EditorDocument({{500, 2, 0}, {0, 0, 0}, {0, 1, 0}, {1000, 4, 750}});
}

std::vector<std::int64_t> times(const EditorDocument &document)
{
    std::vector<std::int64_t> result;
    for (const EditorNote &note : document.notes()) {
        result.push_back(note.time);
    }
    return result;
}

}  // namespace

TEST_CASE("the grid maps times to columns and back")
{
    CHECK(kGrid.slotMs() == 250);
    CHECK(kGrid.columnOf(0) == 0);
    CHECK(kGrid.columnOf(124) == 0);
    CHECK(kGrid.columnOf(125) == 1);
    CHECK(kGrid.timeOf(3) == 750);
    CHECK(kGrid.columnBegin(1) == 125);
    CHECK(kGrid.onGrid(501));
    CHECK_FALSE(kGrid.onGrid(510));
    CHECK(kGrid.place(749.4) == 750);
    CHECK(kGrid.place(780) == 780);

    const TimeGrid odd(140, 1);
    for (int column = 0; column < 200; ++column) {
        CHECK(odd.columnOf(static_cast<double>(odd.timeOf(column))) == column);
        CHECK(odd.onGrid(static_cast<double>(odd.timeOf(column))));
    }
}

TEST_CASE("notes are sorted, unique and keep their hold")
{
    const EditorDocument document({{500, 2, 0}, {0, 1, 0}, {500, 2, 300}, {0, 20, 0}, {-5, 1, 0}});
    REQUIRE(document.notes().size() == 2);
    CHECK(document.notes()[0] == EditorNote{0, 1, 0});
    CHECK(document.notes()[1] == EditorNote{500, 2, 300});
    CHECK(document.hasHoldNotes());
    CHECK(document.endTime() == 800);
}

TEST_CASE("notes come from playback notes when the sheet has them")
{
    StoredSheet sheet;
    sheet.keyMap = KeyMap({{0, {"y"}}, {400, {"u", "i"}}, {2000, {}}});
    auto fromKeyMap = EditorDocument::notesFrom(sheet);
    REQUIRE(fromKeyMap.size() == 3);
    CHECK(fromKeyMap[1] == EditorNote{400, 1, 0});

    sheet.playbackNotes = std::vector<PlaybackNote>{{100, "/", 500}, {0, "h", 0}};
    auto fromPlayback = EditorDocument::notesFrom(sheet);
    REQUIRE(fromPlayback.size() == 2);
    CHECK(fromPlayback[0] == EditorNote{0, 5, 0});
    CHECK(fromPlayback[1] == EditorNote{100, 14, 500});
}

TEST_CASE("the grid follows the sheet BPM when the notes fit it")
{
    const GridGuess beats = EditorDocument::detectGrid({{0, 0, 0}, {500, 1, 0}, {1500, 2, 0}}, 120);
    CHECK(beats.steady);
    CHECK(beats.grid.slotMs() == 500);
    CHECK(beats.grid.originMs() == 0);

    const GridGuess halves = EditorDocument::detectGrid({{0, 0, 0}, {250, 1, 0}, {1000, 2, 0}, {1250, 3, 0}}, 120);
    CHECK(halves.steady);
    CHECK(halves.grid.slotMs() == 250);
    CHECK(halves.grid.subdivision() == 2);

    const GridGuess empty = EditorDocument::detectGrid({}, 120);
    CHECK(empty.steady);
    CHECK(empty.grid.bpm() == 120);
}

TEST_CASE("the grid finds the real beat when the BPM is wrong or the notes start late")
{
    std::vector<EditorNote> notes;
    for (int i = 0; i < 200; ++i) {
        const double exact = 37 + i * (60000.0 / 220 / 2);
        notes.push_back(EditorNote{std::llround(exact), i % 15, 0});
    }
    const GridGuess guess = EditorDocument::detectGrid(notes, 90);
    REQUIRE(guess.steady);
    CHECK(std::fabs(guess.grid.slotMs() - 60000.0 / 220 / 2) < 0.05);
    CHECK(std::fabs(guess.grid.originMs() - 37) < 1.0);
    const EditorDocument document(notes);
    CHECK(document.offGridCount(guess.grid) == 0);
    CHECK(guess.grid.columnOf(37) == 0);
}

TEST_CASE("a sheet without a steady beat keeps free timing")
{
    std::vector<EditorNote> notes;
    std::int64_t time = 0;
    for (int i = 0; i < 120; ++i) {
        time += 90 + (i * 37) % 211;
        notes.push_back(EditorNote{time, i % 15, 0});
    }
    const GridGuess guess = EditorDocument::detectGrid(notes, 220);
    CHECK_FALSE(guess.steady);
    CHECK(guess.grid.slotMs() >= 80);
    CHECK(guess.grid.slotMs() <= 500);
}

TEST_CASE("a grid with an offset maps columns around its origin")
{
    const TimeGrid grid(120, 2, 40);
    CHECK(grid.timeOf(0) == 40);
    CHECK(grid.timeOf(2) == 540);
    CHECK(grid.columnOf(0) == 0);
    CHECK(grid.columnOf(540) == 2);
    CHECK(grid.onGrid(290));
    EditorDocument document;
    CHECK(document.toggleInColumn(1, 3, grid));
    CHECK(document.notes().front().time == 290);
}

TEST_CASE("toggling a cell adds a note on the grid or removes the one there")
{
    EditorDocument document;
    CHECK(document.toggleInColumn(3, 6, kGrid));
    CHECK(document.notes() == std::vector<EditorNote>{{750, 6, 0}});
    CHECK(document.keysInColumn(3, kGrid) == std::vector<int>{6});
    CHECK(document.isDirty());

    EditorDocument offGrid({{760, 6, 0}});
    CHECK_FALSE(offGrid.toggleInColumn(3, 6, kGrid));
    CHECK(offGrid.empty());
    CHECK_FALSE(offGrid.toggleInColumn(-1, 0, kGrid));
    CHECK_FALSE(offGrid.toggleInColumn(0, 15, kGrid));
}

TEST_CASE("undo and redo walk through every edit")
{
    EditorDocument document = sample();
    CHECK_FALSE(document.canUndo());
    document.toggleInColumn(0, 3, kGrid);
    document.eraseColumns(2, 2, kGrid);
    REQUIRE(document.undo());
    CHECK(document.find(500, 2) >= 0);
    REQUIRE(document.undo());
    CHECK(document.find(0, 3) < 0);
    CHECK_FALSE(document.isDirty());
    CHECK_FALSE(document.undo());
    REQUIRE(document.redo());
    CHECK(document.find(0, 3) >= 0);
    document.toggleInColumn(5, 5, kGrid);
    CHECK_FALSE(document.canRedo());
    document.discard();
    CHECK_FALSE(document.isDirty());
    REQUIRE(document.undo());
    CHECK(document.isDirty());
}

TEST_CASE("notes move and change hold but never land on another note")
{
    EditorDocument document = sample();
    CHECK(document.moveNote(500, 2, 750, 3));
    CHECK(document.find(750, 3) >= 0);
    CHECK_FALSE(document.moveNote(750, 3, 0, 0));
    CHECK_FALSE(document.moveNote(750, 3, 750, 15));
    CHECK_FALSE(document.moveNote(9, 9, 0, 0));
    CHECK(document.setHold(750, 3, 500));
    CHECK(document.notes()[static_cast<std::size_t>(document.find(750, 3))].hold == 500);
    CHECK_FALSE(document.setHold(750, 3, 500));
    CHECK(document.setHold(750, 3, -20));
    CHECK(document.notes()[static_cast<std::size_t>(document.find(750, 3))].hold == 0);
}

TEST_CASE("inserting and deleting columns moves the later notes")
{
    EditorDocument document = sample();
    CHECK(document.insertColumns(2, 2, kGrid));
    CHECK(times(document) == std::vector<std::int64_t>{0, 0, 1000, 1500});
    CHECK(document.notes().back().hold == 750);
    CHECK(document.deleteColumns(2, 3, kGrid));
    CHECK(times(document) == std::vector<std::int64_t>{0, 0, 500, 1000});
    CHECK(document.deleteColumns(2, 2, kGrid));
    CHECK(times(document) == std::vector<std::int64_t>{0, 0, 750});
    CHECK_FALSE(document.insertColumns(40, 1, kGrid));
    CHECK_FALSE(document.deleteColumns(40, 41, kGrid));
}

TEST_CASE("off-grid notes keep their offset when columns move")
{
    EditorDocument document({{510, 0, 0}});
    CHECK(document.insertColumns(0, 1, kGrid));
    CHECK(document.notes().front().time == 760);
    CHECK(document.offGridCount(kGrid) == 1);
    CHECK(document.snapColumns(0, 10, kGrid));
    CHECK(document.notes().front().time == 750);
    CHECK(document.offGridCount(kGrid) == 0);
    CHECK_FALSE(document.snapColumns(0, 10, kGrid));
}

TEST_CASE("snapping merges notes that land on the same spot")
{
    EditorDocument document({{498, 3, 0}, {505, 3, 400}});
    CHECK(document.snapColumns(0, 4, kGrid));
    CHECK(document.notes() == std::vector<EditorNote>{{500, 3, 400}});
}

TEST_CASE("copied columns paste at a column, inserted or on top")
{
    EditorDocument document = sample();
    const ColumnClip clip = document.copyColumns(0, 1, kGrid);
    REQUIRE(clip.columns == 2);
    REQUIRE(clip.notes.size() == 2);

    EditorDocument inserted = sample();
    CHECK(inserted.pasteColumns(2, clip, kGrid, true));
    CHECK(times(inserted) == std::vector<std::int64_t>{0, 0, 500, 500, 1000, 1500});

    EditorDocument over = sample();
    CHECK(over.pasteColumns(2, clip, kGrid, false));
    CHECK(times(over) == std::vector<std::int64_t>{0, 0, 500, 500, 500, 1000});
    CHECK_FALSE(over.pasteColumns(2, clip, kGrid, false));
    CHECK_FALSE(over.pasteColumns(0, ColumnClip{}, kGrid, true));
}

TEST_CASE("notes shift along the keyboard but never off it")
{
    EditorDocument document = sample();
    CHECK(document.shiftKeys(0, 0, 1, kGrid));
    CHECK(document.keysInColumn(0, kGrid) == std::vector<int>{1, 2});
    CHECK(document.shiftKeys(0, 0, -1, kGrid));
    CHECK_FALSE(document.shiftKeys(0, 0, -1, kGrid));
    CHECK_FALSE(document.shiftKeys(1, 1, 1, kGrid));

    EditorDocument top({{0, 14, 0}});
    CHECK_FALSE(top.shiftKeys(0, 0, 1, kGrid));
    CHECK_FALSE(top.canUndo());
}

TEST_CASE("edited notes become a payload with a key map and holds")
{
    const EditorDocument document = sample();
    const std::vector<SongNote> songNotes = document.songNotes();
    REQUIRE(songNotes.size() == 4);
    CHECK(songNotes.back() == SongNote{1000, "1Key4", 750});
    auto payload = payloadFromSongNotes(songNotes);
    REQUIRE(payload);
    const auto &entries = payload.value().keyMap.entries();
    CHECK(entries.front().time == 0);
    std::vector<std::string> first = entries.front().keys;
    std::sort(first.begin(), first.end());
    CHECK(first == std::vector<std::string>{"u", "y"});
    CHECK(entries.back().time == 2000);
    CHECK(entries.back().keys.empty());
    CHECK_FALSE(payloadFromSongNotes({}));
}

TEST_CASE("selected columns move earlier or later and merge with what is there")
{
    EditorDocument document = sample();
    CHECK(document.moveColumns(2, 2, 1, kGrid));
    CHECK(document.find(750, 2) >= 0);
    CHECK(document.find(500, 2) < 0);
    CHECK(document.moveColumns(0, 0, 4, kGrid));
    CHECK(document.find(1000, 0) >= 0);
    CHECK(document.find(1000, 4) >= 0);
    CHECK_FALSE(document.moveColumns(1, 1, -2, kGrid));
    CHECK_FALSE(document.moveColumns(3, 3, 0, kGrid));
    CHECK_FALSE(document.moveColumns(20, 21, 1, kGrid));

    EditorDocument offGrid({{510, 0, 0}});
    CHECK(offGrid.moveColumns(2, 2, 1, kGrid));
    CHECK(offGrid.notes().front().time == 760);
}

TEST_CASE("holds grow and shrink one column at a time")
{
    EditorDocument document = sample();
    CHECK(document.changeHold(4, 4, 1, kGrid));
    CHECK(document.longestHold(4, 4, kGrid) == 1000);
    CHECK(document.changeHold(4, 4, -10, kGrid));
    CHECK(document.longestHold(4, 4, kGrid) == 0);
    CHECK_FALSE(document.changeHold(4, 4, -1, kGrid));
    CHECK(document.changeHold(0, 0, 2, kGrid));
    CHECK(document.notes()[0].hold == 500);
    CHECK(document.notes()[1].hold == 500);
    CHECK_FALSE(document.changeHold(10, 12, 1, kGrid));
}

TEST_CASE("free timing picks columns small enough to keep notes apart")
{
    std::vector<EditorNote> notes;
    std::int64_t time = 0;
    for (int i = 0; i < 200; ++i) {
        time += i % 4 == 0 ? 90 : 300 + (i * 53) % 97;
        notes.push_back(EditorNote{time, i % 15, 0});
    }
    const GridGuess guess = EditorDocument::detectGrid(notes, 220);
    REQUIRE_FALSE(guess.steady);
    int crowded = 0;
    for (std::size_t i = 1; i < notes.size(); ++i) {
        crowded += guess.grid.columnOf(static_cast<double>(notes[i].time)) ==
                           guess.grid.columnOf(static_cast<double>(notes[i - 1].time))
                       ? 1
                       : 0;
    }
    CHECK(crowded <= static_cast<int>(notes.size() * 0.15));
}
