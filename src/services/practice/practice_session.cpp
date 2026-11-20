#include "services/practice/practice_session.hpp"

#include <algorithm>
#include <cmath>

#include "model/key_table.hpp"

namespace sky {

double PracticeStats::accuracy() const
{
    const int total = judged();
    if (total == 0) {
        return 100.0;
    }
    return (perfect + good * PracticeSession::kGoodWeight) * 100.0 / total;
}

std::string PracticeStats::grade() const
{
    const double value = accuracy();
    if (value >= 95) {
        return "S";
    }
    if (value >= 88) {
        return "A";
    }
    if (value >= 75) {
        return "B";
    }
    if (value >= 60) {
        return "C";
    }
    return "D";
}

PracticeSession::PracticeSession(const KeyMap &keyMap)
{
    for (const KeyMapEntry &entry : keyMap.entries()) {
        PracticeChord chord;
        chord.time = entry.time;
        std::vector<int> keys;
        for (const std::string &name : entry.keys) {
            const int key = autoKeyIndex(name);
            if (key >= 0 && std::find(keys.begin(), keys.end(), key) == keys.end()) {
                keys.push_back(key);
            }
        }
        if (keys.empty()) {
            continue;
        }
        const int chordIndex = static_cast<int>(m_chords.size());
        for (const int key : keys) {
            chord.notes.push_back(static_cast<int>(m_notes.size()));
            m_notes.push_back(PracticeNote{entry.time, key, chordIndex, PracticeNote::State::Pending});
        }
        m_chords.push_back(std::move(chord));
    }
    reset(1.0);
}

double PracticeSession::progress() const
{
    const double span = lastTime() - firstTime();
    if (span <= 0) {
        return m_finished ? 1.0 : 0.0;
    }
    return std::clamp((m_time - firstTime()) / span, 0.0, 1.0);
}

int PracticeSession::activeChord() const
{
    for (std::size_t i = 0; i < m_chords.size(); ++i) {
        for (const int note : m_chords[i].notes) {
            if (m_notes[static_cast<std::size_t>(note)].state == PracticeNote::State::Pending) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

void PracticeSession::reset(double speed)
{
    for (PracticeNote &note : m_notes) {
        note.state = PracticeNote::State::Pending;
    }
    m_stats = PracticeStats{};
    m_time = firstTime() - kIntroMs * speed;
    m_waiting = false;
    m_finished = m_notes.empty();
}

void PracticeSession::setWaitMode(bool enabled)
{
    m_waitMode = enabled;
    if (!enabled) {
        m_waiting = false;
    }
}

void PracticeSession::resolve(int index, PracticeNote::State state)
{
    PracticeNote &note = m_notes[static_cast<std::size_t>(index)];
    note.state = state;
    if (state == PracticeNote::State::Missed) {
        ++m_stats.miss;
        m_stats.combo = 0;
        return;
    }
    if (state == PracticeNote::State::Perfect) {
        ++m_stats.perfect;
    } else {
        ++m_stats.good;
    }
    ++m_stats.combo;
    m_stats.bestCombo = std::max(m_stats.bestCombo, m_stats.combo);
}

void PracticeSession::checkFinished(double speed)
{
    if (m_finished || activeChord() >= 0) {
        return;
    }
    if (m_time >= lastTime() + kOutroMs * speed) {
        m_finished = true;
    }
}

std::vector<PracticeHit> PracticeSession::advance(double realMs, double speed)
{
    std::vector<PracticeHit> missed;
    if (m_finished) {
        return missed;
    }
    const double target = m_time + std::max(0.0, realMs) * speed;
    if (m_waitMode) {
        const int chord = activeChord();
        if (chord >= 0 && target >= m_chords[static_cast<std::size_t>(chord)].time) {
            m_time = m_chords[static_cast<std::size_t>(chord)].time;
            m_waiting = true;
        } else {
            m_time = target;
            m_waiting = false;
        }
    } else {
        m_time = target;
        const double window = kGoodMs * speed;
        for (std::size_t i = 0; i < m_notes.size(); ++i) {
            const PracticeNote &note = m_notes[i];
            if (note.state == PracticeNote::State::Pending && note.time + window < m_time) {
                resolve(static_cast<int>(i), PracticeNote::State::Missed);
                missed.push_back(PracticeHit{note.key, PracticeJudgement::Miss, static_cast<int>(i)});
            }
        }
    }
    checkFinished(speed);
    return missed;
}

PracticeHit PracticeSession::press(int key, double speed)
{
    PracticeHit hit{key, PracticeJudgement::Wrong, -1};
    if (m_finished || key < 0 || key >= kSkyKeyCount) {
        return hit;
    }
    if (m_waitMode) {
        const int chord = activeChord();
        if (chord < 0) {
            return hit;
        }
        const PracticeChord &current = m_chords[static_cast<std::size_t>(chord)];
        if (m_time < current.time - kGoodMs * speed) {
            ++m_stats.wrong;
            return hit;
        }
        for (const int index : current.notes) {
            const PracticeNote &note = m_notes[static_cast<std::size_t>(index)];
            if (note.key == key && note.state == PracticeNote::State::Pending) {
                resolve(index, PracticeNote::State::Perfect);
                hit.judgement = PracticeJudgement::Perfect;
                hit.note = index;
                if (activeChord() != chord) {
                    m_waiting = false;
                }
                return hit;
            }
        }
        ++m_stats.wrong;
        return hit;
    }

    const double good = kGoodMs * speed;
    int best = -1;
    double bestDistance = good;
    for (std::size_t i = 0; i < m_notes.size(); ++i) {
        const PracticeNote &note = m_notes[i];
        if (note.key != key || note.state != PracticeNote::State::Pending) {
            continue;
        }
        const double distance = std::abs(note.time - m_time);
        if (distance <= bestDistance) {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
        if (note.time > m_time + good) {
            break;
        }
    }
    if (best < 0) {
        ++m_stats.wrong;
        return hit;
    }
    const bool perfect = bestDistance <= kPerfectMs * speed;
    resolve(best, perfect ? PracticeNote::State::Perfect : PracticeNote::State::Good);
    hit.judgement = perfect ? PracticeJudgement::Perfect : PracticeJudgement::Good;
    hit.note = best;
    return hit;
}

}  // namespace sky
