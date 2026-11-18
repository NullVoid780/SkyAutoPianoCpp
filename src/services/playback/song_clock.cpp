#include "services/playback/song_clock.hpp"

#include <algorithm>
#include <chrono>

#include "core/precise_sleep.hpp"

namespace sky {

namespace {

double steadyNowMs()
{
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

SongClock::SongClock(double startSongTime, TimeSource now, Sleeper sleep)
    : m_now(now ? std::move(now) : TimeSource(steadyNowMs)),
      m_sleep(sleep ? std::move(sleep) : Sleeper(preciseSleep)),
      m_songTime(startSongTime),
      m_lastRealTime(m_now())
{
}

void SongClock::advance(double speed)
{
    const double now = m_now();
    const double delta = now - m_lastRealTime;
    if (delta > 0) {
        m_songTime += delta * speed;
        m_lastRealTime = now;
    }
}

bool SongClock::waitUntil(double targetSongTime, const SpeedSource &speed, const ActiveCheck &active)
{
    while (m_songTime < targetSongTime) {
        if (!active()) {
            return false;
        }
        advance(speed());
        const double remaining = targetSongTime - m_songTime;
        if (remaining <= 0) {
            break;
        }
        const double sleepMs = std::min(remaining / speed(), kMaxSliceMs);
        m_sleep(sleepMs > 1 ? sleepMs : 0.0);
    }
    return active();
}

}  // namespace sky
