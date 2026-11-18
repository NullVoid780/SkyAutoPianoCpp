#include "core/precise_sleep.hpp"

#include <windows.h>
#include <timeapi.h>

#include <algorithm>
#include <cmath>

namespace sky {

namespace {

constexpr double kTimerThresholdMs = 1.5;
constexpr double kSpinWindowMs = 1.0;

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

class ThreadTimer {
public:
    ThreadTimer()
    {
        m_handle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS);
    }

    ~ThreadTimer()
    {
        if (m_handle) {
            CloseHandle(m_handle);
        }
    }

    ThreadTimer(const ThreadTimer &) = delete;
    ThreadTimer &operator=(const ThreadTimer &) = delete;

    bool wait(double milliseconds)
    {
        if (!m_handle) {
            return false;
        }
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>(milliseconds * 10000.0);
        if (!SetWaitableTimerEx(m_handle, &due, 0, nullptr, nullptr, nullptr, 0)) {
            return false;
        }
        return WaitForSingleObject(m_handle, INFINITE) == WAIT_OBJECT_0;
    }

private:
    HANDLE m_handle = nullptr;
};

LONGLONG counterFrequency()
{
    static const LONGLONG frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value.QuadPart;
    }();
    return frequency;
}

LONGLONG counterNow()
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

}  // namespace

void preciseSleep(double milliseconds)
{
    if (!(milliseconds > 0.0)) {
        SwitchToThread();
        return;
    }
    const LONGLONG deadline =
        counterNow() + static_cast<LONGLONG>(milliseconds * counterFrequency() / 1000.0);

    if (milliseconds >= kTimerThresholdMs) {
        thread_local ThreadTimer timer;
        const double coarse = milliseconds - kSpinWindowMs;
        if (!timer.wait(coarse)) {
            Sleep(static_cast<DWORD>(std::floor(coarse)));
        }
    }
    while (counterNow() < deadline) {
        YieldProcessor();
    }
}

bool preciseSleepWhile(double milliseconds, const std::function<bool()> &keepWaiting, double sliceMs)
{
    const LONGLONG deadline =
        counterNow() + static_cast<LONGLONG>(milliseconds * counterFrequency() / 1000.0);
    while (true) {
        if (!keepWaiting()) {
            return false;
        }
        const double remaining = static_cast<double>(deadline - counterNow()) * 1000.0 / counterFrequency();
        if (remaining <= 0.0) {
            return true;
        }
        preciseSleep(std::min(remaining, sliceMs));
    }
}

TimerResolutionScope::TimerResolutionScope()
    : m_active(timeBeginPeriod(1) == TIMERR_NOERROR)
{
}

TimerResolutionScope::~TimerResolutionScope()
{
    if (m_active) {
        timeEndPeriod(1);
    }
}

}  // namespace sky
