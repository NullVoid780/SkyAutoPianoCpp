#pragma once

#include <atomic>

#include "skyinput/sender.hpp"

namespace skyinput {

class HardwareSender final : public ISender {
public:
    HardwareSender() = default;
    explicit HardwareSender(WindowHandle target);

    Result<void> toggleKey(KeyCode key, bool down) override;
    Result<void> printChars(std::wstring_view text) override;
    Result<void> toggleMouse(MouseButton button, bool down) override;
    Result<void> move(Point point, bool absolute) override;
    Result<void> scrollWheel(int amount) override;
    Point cursorPosition() const override;
    Point lastCoords() const override;
    void setTarget(WindowHandle window) override;
    WindowHandle target() const override;

private:
    std::atomic<WindowHandle> m_target{nullptr};
};

}  // namespace skyinput
