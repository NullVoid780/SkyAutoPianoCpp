#pragma once

#include "skyinput/geometry.hpp"
#include "skyinput/key_code.hpp"
#include "skyinput/mouse_button.hpp"

namespace skyinput {

Size screenSize();
bool isButtonPressed(KeyCode key);
bool isButtonPressed(MouseButton button);

}  // namespace skyinput
