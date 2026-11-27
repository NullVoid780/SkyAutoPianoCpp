#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "model/note.hpp"
#include "model/stored_sheet.hpp"

namespace sky {

struct EditorNote {
    std::int64_t time = 0;
    int key = 0;
    std::int64_t hold = 0;

    bool operator==(const EditorNote &) const = default;
};

class TimeGrid {
public:
    static constexpr double kOnGridMs = 2.0;

    TimeGrid() = default;
    TimeGrid(double bpm, int subdivision, double originMs = 0.0);

    double slotMs() const { return m_slotMs; }
    double bpm() const { return m_bpm; }
    int subdivision() const { return m_subdivision; }
    double originMs() const { return m_originMs; }
    int columnOf(double time) const;
    std::int64_t timeOf(int column) const;
    std::int64_t columnBegin(int column) const;
    double offsetOf(double time) const;
    bool onGrid(double time) const;
    std::int64_t place(double exactTime) const;

private:
    double m_bpm = 120.0;
    int m_subdivision = 1;
    double m_slotMs = 500.0;
    double m_originMs = 0.0;
};

struct GridGuess {
    TimeGrid grid;
    bool steady = true;
};

struct ColumnClip {
    std::vector<EditorNote> notes;
    int columns = 0;

    bool empty() const { return columns <= 0; }
};

class EditorDocument {
public:
    static constexpr std::size_t kHistoryLimit = 500;
    static constexpr int kKeyCount = 15;

    EditorDocument() = default;
    explicit EditorDocument(std::vector<EditorNote> notes);
    static std::vector<EditorNote> notesFrom(const StoredSheet &sheet);
    static GridGuess detectGrid(const std::vector<EditorNote> &notes, double sheetBpm);

    const std::vector<EditorNote> &notes() const { return m_current; }
    bool empty() const { return m_current.empty(); }
    std::int64_t endTime() const;
    bool hasHoldNotes() const;
    int find(std::int64_t time, int key) const;
    std::pair<int, int> indexRange(std::int64_t from, std::int64_t to) const;
    std::vector<int> keysInColumn(int column, const TimeGrid &grid) const;
    int lastColumn(const TimeGrid &grid) const;
    int offGridCount(const TimeGrid &grid) const;
    std::vector<SongNote> songNotes() const;

    bool toggleInColumn(int column, int key, const TimeGrid &grid);
    bool moveNote(std::int64_t time, int key, std::int64_t newTime, int newKey);
    bool setHold(std::int64_t time, int key, std::int64_t hold);
    bool insertColumns(int at, int count, const TimeGrid &grid);
    bool deleteColumns(int first, int last, const TimeGrid &grid);
    bool eraseColumns(int first, int last, const TimeGrid &grid);
    ColumnClip copyColumns(int first, int last, const TimeGrid &grid) const;
    bool pasteColumns(int at, const ColumnClip &clip, const TimeGrid &grid, bool insert);
    bool shiftKeys(int first, int last, int delta, const TimeGrid &grid);
    bool snapColumns(int first, int last, const TimeGrid &grid);
    bool moveColumns(int first, int last, int delta, const TimeGrid &grid);
    bool changeHold(int first, int last, int deltaColumns, const TimeGrid &grid);
    std::int64_t longestHold(int first, int last, const TimeGrid &grid) const;

    bool canUndo() const { return !m_undo.empty(); }
    bool canRedo() const { return !m_redo.empty(); }
    bool undo();
    bool redo();

    bool isDirty() const { return m_current != m_saved; }
    void discard();
    void markSaved() { m_saved = m_current; }

private:
    std::pair<int, int> columnRange(int first, int last, const TimeGrid &grid) const;
    void commit(std::vector<EditorNote> notes);
    static void normalize(std::vector<EditorNote> &notes);

    std::vector<EditorNote> m_current;
    std::vector<EditorNote> m_saved;
    std::vector<std::vector<EditorNote>> m_undo;
    std::vector<std::vector<EditorNote>> m_redo;
};

}  // namespace sky
