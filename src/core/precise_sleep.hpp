#pragma once

#include <functional>

namespace sky {

void preciseSleep(double milliseconds);
bool preciseSleepWhile(double milliseconds, const std::function<bool()> &keepWaiting, double sliceMs = 5.0);

class TimerResolutionScope {
public:
    TimerResolutionScope();
    ~TimerResolutionScope();

    TimerResolutionScope(const TimerResolutionScope &) = delete;
    TimerResolutionScope &operator=(const TimerResolutionScope &) = delete;

private:
    bool m_active = false;
};

}  // namespace sky
