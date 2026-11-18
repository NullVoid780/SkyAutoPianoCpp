#pragma once

#include <string_view>

#include "skyinput/error.hpp"
#include "skyinput/geometry.hpp"
#include "skyinput/key_code.hpp"
#include "skyinput/mouse_button.hpp"

namespace skyinput {

class ISender {
public:
    virtual ~ISender() = default;

    virtual Result<void> toggleKey(KeyCode key, bool down) = 0;
    virtual Result<void> printChars(std::wstring_view text) = 0;
    virtual Result<void> toggleMouse(MouseButton button, bool down) = 0;
    virtual Result<void> move(Point point, bool absolute) = 0;
    virtual Result<void> scrollWheel(int amount) = 0;
    virtual Point cursorPosition() const = 0;
    virtual Point lastCoords() const = 0;
    virtual void setTarget(WindowHandle window) = 0;
    virtual WindowHandle target() const = 0;
};

}  // namespace skyinput
