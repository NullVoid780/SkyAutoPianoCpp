#pragma once

#include <optional>
#include <string_view>

namespace skyinput {

enum class MouseButton { Left, Right, Middle, X1, X2 };

inline std::optional<MouseButton> mouseButtonFromName(std::string_view name)
{
    if (name == "left") {
        return MouseButton::Left;
    }
    if (name == "right") {
        return MouseButton::Right;
    }
    if (name == "middle") {
        return MouseButton::Middle;
    }
    if (name == "x1") {
        return MouseButton::X1;
    }
    if (name == "x2") {
        return MouseButton::X2;
    }
    return std::nullopt;
}

inline std::string_view mouseButtonName(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:
        return "left";
    case MouseButton::Right:
        return "right";
    case MouseButton::Middle:
        return "middle";
    case MouseButton::X1:
        return "x1";
    case MouseButton::X2:
        return "x2";
    }
    return "left";
}

}  // namespace skyinput
