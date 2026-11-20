#pragma once

#include <string>
#include <vector>

#include "model/key_map.hpp"

namespace sky {

enum class PracticeJudgement { Perfect, Good, Miss, Wrong };

struct PracticeNote {
    enum class State { Pending, Perfect, Good, Missed };

    double time = 0;
    int key = 0;
    int chord = 0;
    State state = State::Pending;
};

struct PracticeChord {
    double time = 0;
    std::vector<int> notes;
};

struct PracticeHit {
    int key = -1;
    PracticeJudgement judgement = PracticeJudgement::Wrong;
    int note = -1;
};

struct PracticeStats {
    int perfect = 0;
    int good = 0;
    int miss = 0;
    int wrong = 0;
    int combo = 0;
    int bestCombo = 0;

    int judged() const { return perfect + good + miss; }
    double accuracy() const;
    std::string grade() const;
};

class PracticeSession {
public:
    static constexpr double kPerfectMs = 70;
    static constexpr double kGoodMs = 150;
    static constexpr double kIntroMs = 1800;
    static constexpr double kOutroMs = 700;
    static constexpr double kGoodWeight = 0.6;

    explicit PracticeSession(const KeyMap &keyMap = {});

    bool empty() const { return m_notes.empty(); }
    double firstTime() const { return m_chords.empty() ? 0.0 : m_chords.front().time; }
    double lastTime() const { return m_chords.empty() ? 0.0 : m_chords.back().time; }
    double time() const { return m_time; }
    double progress() const;
    bool finished() const { return m_finished; }
    bool waiting() const { return m_waiting; }
    bool waitMode() const { return m_waitMode; }
    int activeChord() const;

    const std::vector<PracticeNote> &notes() const { return m_notes; }
    const std::vector<PracticeChord> &chords() const { return m_chords; }
    const PracticeStats &stats() const { return m_stats; }

    void reset(double speed);
    void setWaitMode(bool enabled);
    std::vector<PracticeHit> advance(double realMs, double speed);
    PracticeHit press(int key, double speed);

private:
    void resolve(int index, PracticeNote::State state);
    void checkFinished(double speed);

    std::vector<PracticeNote> m_notes;
    std::vector<PracticeChord> m_chords;
    PracticeStats m_stats;
    double m_time = 0;
    bool m_waitMode = false;
    bool m_waiting = false;
    bool m_finished = false;
};

}  // namespace sky
