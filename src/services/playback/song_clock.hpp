#pragma once

#include <functional>

namespace sky {

inline constexpr double kMaxSliceMs = 5.0;

class SongClock {
public:
    using TimeSource = std::function<double()>;
    using Sleeper = std::function<void(double)>;
    using SpeedSource = std::function<double()>;
    using ActiveCheck = std::function<bool()>;

    explicit SongClock(double startSongTime, TimeSource now = {}, Sleeper sleep = {});

    double songTime() const { return m_songTime; }
    void advance(double speed);
    bool waitUntil(double targetSongTime, const SpeedSource &speed, const ActiveCheck &active);

private:
    TimeSource m_now;
    Sleeper m_sleep;
    double m_songTime;
    double m_lastRealTime;
};

}  // namespace sky
