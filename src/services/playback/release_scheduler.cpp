#include "services/playback/release_scheduler.hpp"

#include <windows.h>

#include <exception>

#include "core/logger.hpp"
#include "core/precise_sleep.hpp"

namespace sky {

ReleaseScheduler::ReleaseScheduler(skyinput::Keyboard &keyboard)
    : m_keyboard(keyboard), m_thread([this] { run(); })
{
}

ReleaseScheduler::~ReleaseScheduler()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopping = true;
    }
    m_wake.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    releaseAllNow();
}

void ReleaseScheduler::schedule(skyinput::KeyCode key, Clock::time_point releaseAt)
{
    {
        std::lock_guard lock(m_mutex);
        m_queue.push(PendingRelease{releaseAt, m_sequence++, key});
    }
    m_wake.notify_all();
}

void ReleaseScheduler::releaseAllNow()
{
    std::vector<skyinput::KeyCode> keys;
    {
        std::lock_guard lock(m_mutex);
        while (!m_queue.empty()) {
            keys.push_back(m_queue.top().key);
            m_queue.pop();
        }
    }
    for (skyinput::KeyCode key : keys) {
        (void)m_keyboard.toggleKey(key, false);
    }
    m_wake.notify_all();
}

std::size_t ReleaseScheduler::pending() const
{
    std::lock_guard lock(m_mutex);
    return m_queue.size();
}

void ReleaseScheduler::run()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    SetThreadDescription(GetCurrentThread(), L"release");
    TimerResolutionScope resolution;
    try {
        std::unique_lock lock(m_mutex);
        while (!m_stopping) {
            if (m_queue.empty()) {
                m_wake.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
                continue;
            }
            const PendingRelease next = m_queue.top();
            const auto now = Clock::now();
            if (next.at > now) {
                const double remainingMs = std::chrono::duration<double, std::milli>(next.at - now).count();
                if (remainingMs > 2.0) {
                    m_wake.wait_for(lock, std::chrono::duration<double, std::milli>(remainingMs - 1.0));
                    continue;
                }
                lock.unlock();
                preciseSleep(remainingMs);
                lock.lock();
                continue;
            }
            m_queue.pop();
            lock.unlock();
            (void)m_keyboard.toggleKey(next.key, false);
            lock.lock();
        }
    } catch (const std::exception &error) {
        Logger::error("release", error.what());
        m_keyboard.releaseAll();
        std::terminate();
    }
}

}  // namespace sky
