#pragma once

#include <atomic>

#include "skyinput/error.hpp"
#include "skyinput/keyboard.hpp"
#include "skyinput/mouse_button.hpp"
#include "skyinput/sender.hpp"

namespace skyinput {

class Mouse {
public:
    explicit Mouse(ISender &sender);

    Result<void> toggle(MouseButton button, bool down, Duration delay = Duration::zero());
    Result<void> click(MouseButton button = MouseButton::Left, Duration afterPress = kDefaultDelay,
                       Duration afterRelease = Duration::zero());
    Result<void> moveTo(int x, int y, Duration delay = Duration::zero());
    Result<void> move(int dx, int dy, Duration delay = Duration::zero());
    Result<void> humanMoveTo(int x, int y, double speed = 5.0, double deviation = 30.0,
                             Duration delay = Duration::zero(), const std::atomic<bool> *cancel = nullptr);
    Result<void> scrollWheel(int amount, Duration delay = Duration::zero());
    Point position() const;

private:
    ISender &m_sender;
};

}  // namespace skyinput
