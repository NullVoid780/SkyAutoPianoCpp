#include "ui/editor_window/editor_document.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <optional>

#include "model/key_table.hpp"

namespace sky {

namespace {

constexpr double kDefaultBpm = 120.0;
constexpr int kMaxSubdivision = 16;
constexpr double kDetectToleranceMs = 3.0;
constexpr double kDetectShare = 0.06;
constexpr double kSteadyScore = 0.9;
constexpr double kMinSlotMs = 30.0;
constexpr double kMaxSlotMs = 2500.0;
constexpr double kMinFreeSlotMs = 90.0;
constexpr double kChordGapMs = 40.0;
constexpr double kFreeCrowding = 0.12;
constexpr double kFreeShrink = 0.85;
constexpr double kMaxFreeSlotMs = 500.0;
constexpr std::size_t kPhaseSamples = 64;
constexpr std::size_t kIntervalCandidates = 6;
constexpr std::array<int, 6> kSubdivisions = {1, 2, 3, 4, 6, 8};

bool noteLess(const EditorNote &a, const EditorNote &b)
{
    return a.time != b.time ? a.time < b.time : a.key < b.key;
}

struct SlotFit {
    double slot = 0.0;
    double origin = 0.0;
    double score = 0.0;
};

double slotTolerance(double slot)
{
    return std::max(kDetectToleranceMs, slot * kDetectShare);
}

double wrapped(double value, double period)
{
    const double rest = std::fmod(value, period);
    return rest < 0 ? rest + period : rest;
}

double scoreOf(const std::vector<double> &times, double slot, double origin)
{
    const double tolerance = slotTolerance(slot);
    int fitting = 0;
    int crowded = 0;
    long long previous = std::numeric_limits<long long>::min();
    for (double time : times) {
        const long long step = std::llround((time - origin) / slot);
        fitting += std::fabs(time - origin - static_cast<double>(step) * slot) <= tolerance ? 1 : 0;
        crowded += step == previous ? 1 : 0;
        previous = step;
    }
    return static_cast<double>(fitting - crowded) / static_cast<double>(times.size());
}

SlotFit fitSlot(const std::vector<double> &times, double slot)
{
    const double tolerance = slotTolerance(slot);
    const std::size_t stride = std::max<std::size_t>(1, times.size() / kPhaseSamples);
    double origin = 0.0;
    int bestCount = -1;
    for (std::size_t i = 0; i < times.size(); i += stride) {
        const double phase = wrapped(times[i], slot);
        int count = 0;
        for (double time : times) {
            const double distance = std::fabs(wrapped(time - phase + slot / 2, slot) - slot / 2);
            count += distance <= tolerance ? 1 : 0;
        }
        if (count > bestCount) {
            bestCount = count;
            origin = phase;
        }
    }
    for (int pass = 0; pass < 2; ++pass) {
        double sumK = 0;
        double sumT = 0;
        double sumKK = 0;
        double sumKT = 0;
        int count = 0;
        for (double time : times) {
            const double k = std::round((time - origin) / slot);
            if (std::fabs(time - origin - k * slot) <= tolerance) {
                sumK += k;
                sumT += time;
                sumKK += k * k;
                sumKT += k * time;
                ++count;
            }
        }
        const double variance = count * sumKK - sumK * sumK;
        if (count < 2 || variance <= 0) {
            break;
        }
        const double refined = (count * sumKT - sumK * sumT) / variance;
        if (refined < kMinSlotMs || std::fabs(refined - slot) > tolerance) {
            break;
        }
        slot = refined;
        origin = (sumT - slot * sumK) / count;
    }
    return SlotFit{slot, origin, scoreOf(times, slot, origin)};
}

double crowdedShare(const std::vector<double> &times, double slot)
{
    int crowded = 0;
    for (std::size_t i = 1; i < times.size(); ++i) {
        if (times[i] - times[i - 1] >= kChordGapMs &&
            std::llround(times[i] / slot) == std::llround(times[i - 1] / slot)) {
            ++crowded;
        }
    }
    return static_cast<double>(crowded) / static_cast<double>(times.size());
}

int subdivisionFor(double slot, double beat)
{
    int best = kSubdivisions.front();
    double bestDistance = std::numeric_limits<double>::infinity();
    for (int subdivision : kSubdivisions) {
        const double distance = std::fabs(std::log(slot * subdivision / beat));
        if (distance < bestDistance) {
            bestDistance = distance;
            best = subdivision;
        }
    }
    return best;
}

}  // namespace

TimeGrid::TimeGrid(double bpm, int subdivision, double originMs)
    : m_bpm(std::isfinite(bpm) && bpm > 0 ? bpm : kDefaultBpm),
      m_subdivision(std::clamp(subdivision, 1, kMaxSubdivision)),
      m_slotMs(60000.0 / m_bpm / m_subdivision),
      m_originMs(std::isfinite(originMs) ? originMs : 0.0)
{
}

int TimeGrid::columnOf(double time) const
{
    int column = std::max(0, static_cast<int>(std::floor((time - m_originMs) / m_slotMs + 0.5)));
    while (column > 0 && time < static_cast<double>(columnBegin(column))) {
        --column;
    }
    while (time >= static_cast<double>(columnBegin(column + 1))) {
        ++column;
    }
    return column;
}

std::int64_t TimeGrid::timeOf(int column) const
{
    return std::max<std::int64_t>(0, std::llround(m_originMs + column * m_slotMs));
}

std::int64_t TimeGrid::columnBegin(int column) const
{
    return static_cast<std::int64_t>(std::ceil(m_originMs + (column - 0.5) * m_slotMs));
}

double TimeGrid::offsetOf(double time) const
{
    return time - static_cast<double>(timeOf(columnOf(time)));
}

bool TimeGrid::onGrid(double time) const
{
    return std::fabs(offsetOf(time)) <= kOnGridMs;
}

std::int64_t TimeGrid::place(double exactTime) const
{
    const double time = std::max(0.0, exactTime);
    return onGrid(time) ? timeOf(columnOf(time)) : std::llround(time);
}

EditorDocument::EditorDocument(std::vector<EditorNote> notes) : m_current(std::move(notes))
{
    normalize(m_current);
    m_saved = m_current;
}

std::vector<EditorNote> EditorDocument::notesFrom(const StoredSheet &sheet)
{
    std::vector<EditorNote> notes;
    if (sheet.playbackNotes) {
        for (const PlaybackNote &note : *sheet.playbackNotes) {
            const int key = autoKeyIndex(note.key);
            if (key >= 0) {
                notes.push_back(EditorNote{note.time, key, std::max<std::int64_t>(0, note.hold)});
            }
        }
    } else {
        for (const KeyMapEntry &entry : sheet.keyMap.entries()) {
            for (const std::string &name : entry.keys) {
                const int key = autoKeyIndex(name);
                if (key >= 0 && std::isfinite(entry.time)) {
                    notes.push_back(EditorNote{std::llround(entry.time), key, 0});
                }
            }
        }
    }
    normalize(notes);
    return notes;
}

GridGuess EditorDocument::detectGrid(const std::vector<EditorNote> &notes, double sheetBpm)
{
    const bool validBpm = std::isfinite(sheetBpm) && sheetBpm > 0;
    const double beat = 60000.0 / (validBpm ? sheetBpm : kDefaultBpm);
    std::vector<double> times;
    for (const EditorNote &note : notes) {
        if (times.empty() || times.back() != static_cast<double>(note.time)) {
            times.push_back(static_cast<double>(note.time));
        }
    }
    if (times.size() < 2) {
        return GridGuess{TimeGrid(60000.0 / beat, 2), true};
    }

    std::vector<double> candidates;
    if (validBpm) {
        for (int subdivision : kSubdivisions) {
            candidates.push_back(beat / subdivision);
        }
    }
    std::map<long long, int> intervals;
    std::vector<double> gaps;
    for (std::size_t i = 1; i < times.size(); ++i) {
        const double gap = times[i] - times[i - 1];
        if (gap >= kMinSlotMs) {
            ++intervals[std::llround(gap)];
            gaps.push_back(gap);
        }
    }
    std::vector<std::pair<int, long long>> common;
    for (const auto &[gap, count] : intervals) {
        common.emplace_back(count, gap);
    }
    std::sort(common.begin(), common.end(), std::greater<>());
    for (std::size_t i = 0; i < std::min(common.size(), kIntervalCandidates); ++i) {
        for (int divisor = 1; divisor <= 4; ++divisor) {
            candidates.push_back(static_cast<double>(common[i].second) / divisor);
        }
    }

    std::optional<SlotFit> best;
    for (double slot : candidates) {
        if (slot < kMinSlotMs || slot > kMaxSlotMs) {
            continue;
        }
        const SlotFit fit = fitSlot(times, slot);
        if (fit.score >= kSteadyScore && (!best || fit.slot > best->slot + slotTolerance(fit.slot))) {
            best = fit;
        }
    }
    if (!best) {
        double slot = beat / 2;
        if (!gaps.empty()) {
            std::nth_element(gaps.begin(), gaps.begin() + static_cast<std::ptrdiff_t>(gaps.size() / 2), gaps.end());
            slot = std::clamp(gaps[gaps.size() / 2], kMinFreeSlotMs, kMaxFreeSlotMs);
        }
        while (slot * kFreeShrink >= kMinFreeSlotMs && crowdedShare(times, slot) > kFreeCrowding) {
            slot *= kFreeShrink;
        }
        const int subdivision = subdivisionFor(slot, beat);
        return GridGuess{TimeGrid(60000.0 / (slot * subdivision), subdivision), false};
    }
    const int subdivision = subdivisionFor(best->slot, beat);
    double origin = wrapped(best->origin, best->slot);
    if (origin > best->slot / 2) {
        origin -= best->slot;
    }
    return GridGuess{TimeGrid(60000.0 / (best->slot * subdivision), subdivision, origin), true};
}

std::int64_t EditorDocument::endTime() const
{
    std::int64_t end = 0;
    for (const EditorNote &note : m_current) {
        end = std::max(end, note.time + note.hold);
    }
    return end;
}

bool EditorDocument::hasHoldNotes() const
{
    return std::any_of(m_current.begin(), m_current.end(), [](const EditorNote &note) { return note.hold > 0; });
}

int EditorDocument::find(std::int64_t time, int key) const
{
    const EditorNote probe{time, key, 0};
    const auto found = std::lower_bound(m_current.begin(), m_current.end(), probe, noteLess);
    if (found == m_current.end() || found->time != time || found->key != key) {
        return -1;
    }
    return static_cast<int>(found - m_current.begin());
}

std::pair<int, int> EditorDocument::indexRange(std::int64_t from, std::int64_t to) const
{
    const auto byTime = [](const EditorNote &note, std::int64_t time) { return note.time < time; };
    const auto first = std::lower_bound(m_current.begin(), m_current.end(), from, byTime);
    const auto end = std::lower_bound(first, m_current.end(), std::max(from, to), byTime);
    return {static_cast<int>(first - m_current.begin()), static_cast<int>(end - m_current.begin())};
}

std::vector<int> EditorDocument::keysInColumn(int column, const TimeGrid &grid) const
{
    std::vector<int> keys;
    const auto [first, end] = columnRange(column, column, grid);
    for (int i = first; i < end; ++i) {
        const int key = m_current[static_cast<std::size_t>(i)].key;
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
            keys.push_back(key);
        }
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

int EditorDocument::lastColumn(const TimeGrid &grid) const
{
    return m_current.empty() ? -1 : grid.columnOf(static_cast<double>(endTime()));
}

int EditorDocument::offGridCount(const TimeGrid &grid) const
{
    return static_cast<int>(std::count_if(m_current.begin(), m_current.end(), [&grid](const EditorNote &note) {
        return !grid.onGrid(static_cast<double>(note.time));
    }));
}

std::vector<SongNote> EditorDocument::songNotes() const
{
    std::vector<SongNote> notes;
    notes.reserve(m_current.size());
    for (const EditorNote &note : m_current) {
        notes.push_back(SongNote{note.time, songKeyName(note.key), note.hold});
    }
    return notes;
}

bool EditorDocument::toggleInColumn(int column, int key, const TimeGrid &grid)
{
    if (column < 0 || key < 0 || key >= kKeyCount) {
        return false;
    }
    std::vector<EditorNote> notes = m_current;
    const auto [first, end] = columnRange(column, column, grid);
    const auto begin = notes.begin() + first;
    const auto stop = notes.begin() + end;
    const auto kept = std::remove_if(begin, stop, [key](const EditorNote &note) { return note.key == key; });
    if (kept != stop) {
        notes.erase(kept, stop);
        commit(std::move(notes));
        return false;
    }
    notes.push_back(EditorNote{grid.timeOf(column), key, 0});
    commit(std::move(notes));
    return true;
}

bool EditorDocument::moveNote(std::int64_t time, int key, std::int64_t newTime, int newKey)
{
    const int index = find(time, key);
    if (index < 0 || newTime < 0 || newKey < 0 || newKey >= kKeyCount || (newTime == time && newKey == key) ||
        find(newTime, newKey) >= 0) {
        return false;
    }
    std::vector<EditorNote> notes = m_current;
    notes[static_cast<std::size_t>(index)].time = newTime;
    notes[static_cast<std::size_t>(index)].key = newKey;
    commit(std::move(notes));
    return true;
}

bool EditorDocument::setHold(std::int64_t time, int key, std::int64_t hold)
{
    const int index = find(time, key);
    hold = std::max<std::int64_t>(0, hold);
    if (index < 0 || m_current[static_cast<std::size_t>(index)].hold == hold) {
        return false;
    }
    std::vector<EditorNote> notes = m_current;
    notes[static_cast<std::size_t>(index)].hold = hold;
    commit(std::move(notes));
    return true;
}

bool EditorDocument::insertColumns(int at, int count, const TimeGrid &grid)
{
    if (count <= 0 || at < 0) {
        return false;
    }
    std::vector<EditorNote> notes = m_current;
    bool moved = false;
    for (EditorNote &note : notes) {
        const double time = static_cast<double>(note.time);
        if (grid.columnOf(time) >= at) {
            note.time = grid.place(time + count * grid.slotMs());
            moved = true;
        }
    }
    if (!moved) {
        return false;
    }
    commit(std::move(notes));
    return true;
}

bool EditorDocument::deleteColumns(int first, int last, const TimeGrid &grid)
{
    if (first < 0 || last < first) {
        return false;
    }
    const auto [begin, end] = columnRange(first, last, grid);
    const int count = last - first + 1;
    std::vector<EditorNote> notes(m_current.begin(), m_current.begin() + begin);
    for (auto it = m_current.begin() + end; it != m_current.end(); ++it) {
        notes.push_back(EditorNote{grid.place(static_cast<double>(it->time) - count * grid.slotMs()), it->key, it->hold});
    }
    if (begin == end && end == static_cast<int>(m_current.size())) {
        return false;
    }
    commit(std::move(notes));
    return true;
}

bool EditorDocument::eraseColumns(int first, int last, const TimeGrid &grid)
{
    const auto [begin, end] = columnRange(first, last, grid);
    if (begin == end) {
        return false;
    }
    std::vector<EditorNote> notes = m_current;
    notes.erase(notes.begin() + begin, notes.begin() + end);
    commit(std::move(notes));
    return true;
}

ColumnClip EditorDocument::copyColumns(int first, int last, const TimeGrid &grid) const
{
    ColumnClip clip;
    if (first < 0 || last < first) {
        return clip;
    }
    clip.columns = last - first + 1;
    const auto [begin, end] = columnRange(first, last, grid);
    const std::int64_t origin = grid.timeOf(first);
    for (int i = begin; i < end; ++i) {
        EditorNote note = m_current[static_cast<std::size_t>(i)];
        note.time -= origin;
        clip.notes.push_back(note);
    }
    return clip;
}

bool EditorDocument::pasteColumns(int at, const ColumnClip &clip, const TimeGrid &grid, bool insert)
{
    if (clip.empty() || at < 0) {
        return false;
    }
    std::vector<EditorNote> notes = m_current;
    if (insert) {
        for (EditorNote &note : notes) {
            const double time = static_cast<double>(note.time);
            if (grid.columnOf(time) >= at) {
                note.time = grid.place(time + clip.columns * grid.slotMs());
            }
        }
    }
    const std::int64_t origin = grid.timeOf(at);
    for (const EditorNote &note : clip.notes) {
        notes.push_back(EditorNote{grid.place(static_cast<double>(origin + note.time)), note.key, note.hold});
    }
    std::vector<EditorNote> before = m_current;
    commit(std::move(notes));
    return m_current != before;
}

bool EditorDocument::shiftKeys(int first, int last, int delta, const TimeGrid &grid)
{
    const auto [begin, end] = columnRange(first, last, grid);
    if (begin == end || delta == 0) {
        return false;
    }
    for (int i = begin; i < end; ++i) {
        const int key = m_current[static_cast<std::size_t>(i)].key + delta;
        if (key < 0 || key >= kKeyCount) {
            return false;
        }
    }
    std::vector<EditorNote> notes = m_current;
    for (int i = begin; i < end; ++i) {
        notes[static_cast<std::size_t>(i)].key += delta;
    }
    commit(std::move(notes));
    return true;
}

bool EditorDocument::snapColumns(int first, int last, const TimeGrid &grid)
{
    const auto [begin, end] = columnRange(first, last, grid);
    std::vector<EditorNote> notes = m_current;
    bool moved = false;
    for (int i = begin; i < end; ++i) {
        EditorNote &note = notes[static_cast<std::size_t>(i)];
        const std::int64_t snapped = grid.timeOf(grid.columnOf(static_cast<double>(note.time)));
        if (snapped != note.time) {
            note.time = snapped;
            moved = true;
        }
    }
    if (!moved) {
        return false;
    }
    commit(std::move(notes));
    return true;
}

bool EditorDocument::moveColumns(int first, int last, int delta, const TimeGrid &grid)
{
    const auto [begin, end] = columnRange(first, last, grid);
    if (begin == end || delta == 0 || first + delta < 0) {
        return false;
    }
    std::vector<EditorNote> notes = m_current;
    for (int i = begin; i < end; ++i) {
        EditorNote &note = notes[static_cast<std::size_t>(i)];
        note.time = grid.place(static_cast<double>(note.time) + delta * grid.slotMs());
    }
    commit(std::move(notes));
    return true;
}

bool EditorDocument::changeHold(int first, int last, int deltaColumns, const TimeGrid &grid)
{
    const auto [begin, end] = columnRange(first, last, grid);
    if (begin == end || deltaColumns == 0) {
        return false;
    }
    std::vector<EditorNote> notes = m_current;
    bool changed = false;
    for (int i = begin; i < end; ++i) {
        EditorNote &note = notes[static_cast<std::size_t>(i)];
        const long long columns = std::llround(static_cast<double>(note.hold) / grid.slotMs());
        const std::int64_t hold = std::llround(static_cast<double>(std::max(0LL, columns + deltaColumns)) * grid.slotMs());
        if (hold != note.hold) {
            note.hold = hold;
            changed = true;
        }
    }
    if (!changed) {
        return false;
    }
    commit(std::move(notes));
    return true;
}

std::int64_t EditorDocument::longestHold(int first, int last, const TimeGrid &grid) const
{
    const auto [begin, end] = columnRange(first, last, grid);
    std::int64_t longest = 0;
    for (int i = begin; i < end; ++i) {
        longest = std::max(longest, m_current[static_cast<std::size_t>(i)].hold);
    }
    return longest;
}

bool EditorDocument::undo()
{
    if (m_undo.empty()) {
        return false;
    }
    m_redo.push_back(std::move(m_current));
    m_current = std::move(m_undo.back());
    m_undo.pop_back();
    return true;
}

bool EditorDocument::redo()
{
    if (m_redo.empty()) {
        return false;
    }
    m_undo.push_back(std::move(m_current));
    m_current = std::move(m_redo.back());
    m_redo.pop_back();
    return true;
}

void EditorDocument::discard()
{
    if (isDirty()) {
        commit(m_saved);
    }
}

std::pair<int, int> EditorDocument::columnRange(int first, int last, const TimeGrid &grid) const
{
    if (first < 0 || last < first) {
        return {0, 0};
    }
    return indexRange(grid.columnBegin(first), grid.columnBegin(last + 1));
}

void EditorDocument::commit(std::vector<EditorNote> notes)
{
    normalize(notes);
    if (notes == m_current) {
        return;
    }
    m_undo.push_back(std::move(m_current));
    if (m_undo.size() > kHistoryLimit) {
        m_undo.erase(m_undo.begin());
    }
    m_redo.clear();
    m_current = std::move(notes);
}

void EditorDocument::normalize(std::vector<EditorNote> &notes)
{
    std::erase_if(notes, [](const EditorNote &note) { return note.time < 0 || note.key < 0 || note.key >= kKeyCount; });
    for (EditorNote &note : notes) {
        note.hold = std::max<std::int64_t>(0, note.hold);
    }
    std::stable_sort(notes.begin(), notes.end(), noteLess);
    std::vector<EditorNote> unique;
    unique.reserve(notes.size());
    for (const EditorNote &note : notes) {
        if (!unique.empty() && unique.back().time == note.time && unique.back().key == note.key) {
            unique.back().hold = std::max(unique.back().hold, note.hold);
            continue;
        }
        unique.push_back(note);
    }
    notes = std::move(unique);
}

}  // namespace sky
