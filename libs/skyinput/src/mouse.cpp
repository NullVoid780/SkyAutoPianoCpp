#include "skyinput/mouse.hpp"

#include <cmath>
#include <thread>

#include "mouse_curve.hpp"

namespace skyinput {

namespace {

void pause(Duration duration)
{
    if (duration > Duration::zero()) {
        std::this_thread::sleep_for(duration);
    }
}

bool cancelled(const std::atomic<bool> *cancel)
{
    return cancel && cancel->load(std::memory_order_relaxed);
}

struct CurveControls {
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
};

}  // namespace

Mouse::Mouse(ISender &sender) : m_sender(sender) {}

Result<void> Mouse::toggle(MouseButton button, bool down, Duration delay)
{
    Result<void> result = m_sender.toggleMouse(button, down);
    pause(delay);
    return result;
}

Result<void> Mouse::click(MouseButton button, Duration afterPress, Duration afterRelease)
{
    Result<void> pressed = toggle(button, true, afterPress);
    Result<void> released = toggle(button, false, afterRelease);
    return pressed ? released : pressed;
}

Result<void> Mouse::moveTo(int x, int y, Duration delay)
{
    Result<void> result = m_sender.move(Point{x, y}, true);
    pause(delay);
    return result;
}

Result<void> Mouse::move(int dx, int dy, Duration delay)
{
    Result<void> result = m_sender.move(Point{dx, dy}, false);
    pause(delay);
    return result;
}

Result<void> Mouse::scrollWheel(int amount, Duration delay)
{
    Result<void> result = m_sender.scrollWheel(amount);
    pause(delay);
    return result;
}

Point Mouse::position() const
{
    return m_sender.cursorPosition();
}

Result<void> Mouse::humanMoveTo(int xEnd, int yEnd, double speed, double deviation, Duration delay,
                                const std::atomic<bool> *cancel)
{
    deviation /= 100.0;
    const Duration sleepTime{speed >= 1 ? 1 : static_cast<long long>(std::floor(1.0 / speed + 0.5))};
    const Point start = m_sender.lastCoords();
    if (start.x == xEnd && start.y == yEnd) {
        return {};
    }

    const double partLength = curveRandom(50, 200) / 2;
    const double distance = std::sqrt(std::pow(xEnd - start.x, 2.0) + std::pow(yEnd - start.y, 2.0));
    const double partsTotal = std::ceil(distance / partLength);
    const double xPartLength = (xEnd - start.x) / partsTotal;
    const double yPartLength = (yEnd - start.y) / partsTotal;
    const double speedMultiplier = (speed > 1 ? speed + 2 : 3) / partLength;
    const double tremorProbability = speed / 15;

    double partsLeft = partsTotal;
    bool isLastOne = partsTotal == 1;
    double parts = 1;
    double xPartStart = start.x;
    double yPartStart = start.y;
    double xPartEnd = xEnd;
    double yPartEnd = yEnd;
    if (!isLastOne) {
        parts = curveRandom(1, std::ceil(partsTotal / 2));
        xPartEnd = start.x + xPartLength * parts;
        yPartEnd = start.y + yPartLength * parts;
    }

    bool firstSegment = true;
    while (true) {
        CurveControls controls;
        if (firstSegment) {
            controls.x1 = firstCurveControlPoint(xPartStart, xPartEnd, curveRandom(deviation / 2, deviation), 1);
            controls.y1 = firstCurveControlPoint(yPartStart, yPartEnd, curveRandom(deviation / 4, deviation / 3), 1);
            controls.x2 = firstCurveControlPoint(xPartStart, xPartEnd, curveRandom(deviation / 2, deviation), curveSign());
            controls.y2 = firstCurveControlPoint(yPartStart, yPartEnd, curveRandom(deviation / 2, deviation), curveSign());
            firstSegment = false;
        } else {
            controls.x1 = curveControlPoint(xPartStart, xPartEnd, curveRandom(deviation / 3, deviation));
            controls.y1 = curveControlPoint(yPartStart, yPartEnd, curveRandom(deviation / 3, deviation / 2));
            controls.x2 = curveControlPoint(xPartStart, xPartEnd, curveRandom(0, deviation));
            controls.y2 = curveControlPoint(yPartStart, yPartEnd, curveRandom(0, deviation / 2));
        }

        const double dotIterator = speedMultiplier / parts;
        const double count = 1 / dotIterator;
        for (double i = 1; i < count; ++i) {
            if (cancelled(cancel)) {
                return {};
            }
            const double t = i * dotIterator;
            const int x = curvePoint(t, xPartStart, controls.x1, controls.x2, xPartEnd);
            const int y = curvePoint(t, yPartStart, controls.y1, controls.y2, yPartEnd) + curveTremor(tremorProbability);
            if (Result<void> r = moveTo(x, y, sleepTime); !r) {
                return r;
            }
        }
        if (cancelled(cancel)) {
            return {};
        }
        if (isLastOne) {
            return moveTo(xEnd, yEnd, delay);
        }
        if (Result<void> r = moveTo(static_cast<int>(xPartEnd),
                                    static_cast<int>(yPartEnd) + curveTremor(tremorProbability), sleepTime);
            !r) {
            return r;
        }
        partsLeft -= parts;
        xPartStart = xPartEnd;
        yPartStart = yPartEnd;
        if (partsLeft > 2) {
            parts = curveRandom(1, partsLeft - 1);
            xPartEnd += xPartLength * parts;
            yPartEnd += yPartLength * parts;
        } else {
            parts = partsLeft;
            xPartEnd = xEnd;
            yPartEnd = yEnd;
            isLastOne = true;
        }
    }
}

}  // namespace skyinput
