#pragma once

#include <functional>

namespace sky {

inline constexpr double kMaxSliceMs = 5.0;
inline constexpr double kHoldSliceMs = 10.0;

class SongClock {
public:
    using TimeSource = std::function<double()>;
    using Sleeper = std::function<void(double)>;
    using SpeedSource = std::function<double()>;
    using ActiveCheck = std::function<bool()>;
    using HoldCheck = std::function<bool()>;
    using HoldStarted = std::function<void()>;

    explicit SongClock(double startSongTime, TimeSource now = {}, Sleeper sleep = {});

    double songTime() const { return m_songTime; }
    void advance(double speed);
    bool waitUntil(double targetSongTime, const SpeedSource &speed, const ActiveCheck &active,
                   const HoldCheck &hold = {}, const HoldStarted &onHold = {});

private:
    TimeSource m_now;
    Sleeper m_sleep;
    double m_songTime;
    double m_lastRealTime;
    bool m_holding = false;
};

}  // namespace sky
