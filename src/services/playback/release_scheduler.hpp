#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "skyinput/keyboard.hpp"

namespace sky {

class ReleaseScheduler {
public:
    using Clock = std::chrono::steady_clock;

    explicit ReleaseScheduler(skyinput::Keyboard &keyboard);
    ~ReleaseScheduler();

    ReleaseScheduler(const ReleaseScheduler &) = delete;
    ReleaseScheduler &operator=(const ReleaseScheduler &) = delete;

    void schedule(skyinput::KeyCode key, Clock::time_point releaseAt);
    void releaseAllNow();
    std::size_t pending() const;

private:
    struct PendingRelease {
        Clock::time_point at;
        std::uint64_t sequence = 0;
        skyinput::KeyCode key{};

        bool operator>(const PendingRelease &other) const
        {
            return at != other.at ? at > other.at : sequence > other.sequence;
        }
    };

    void run();

    skyinput::Keyboard &m_keyboard;
    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::priority_queue<PendingRelease, std::vector<PendingRelease>, std::greater<>> m_queue;
    std::uint64_t m_sequence = 0;
    bool m_stopping = false;
    std::thread m_thread;
};

}  // namespace sky
