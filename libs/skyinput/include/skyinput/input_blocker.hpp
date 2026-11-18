#pragma once

#include <optional>
#include <string>
#include <vector>

#include "skyinput/error.hpp"
#include "skyinput/key_code.hpp"

namespace skyinput {

enum class MouseAction { Move, WheelForward, WheelBack, Left, Right, Middle, X1, X2 };

struct InputSelection {
    std::optional<std::vector<KeyCode>> keyboard;
    std::optional<std::vector<MouseAction>> mouse;
};

struct BlockedInput {
    enum class Device { Keyboard, Mouse };

    Device device = Device::Keyboard;
    std::string button;
    bool down = false;
};

class InputBlocker {
public:
    static Result<void> disable(const InputSelection &selection = {});
    static std::vector<BlockedInput> enable(const InputSelection &selection = {});
};

}  // namespace skyinput
