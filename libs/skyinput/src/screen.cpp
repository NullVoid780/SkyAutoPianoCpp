#include "skyinput/screen.hpp"

#include <windows.h>

namespace skyinput {

namespace {

int mouseVirtualKey(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:
        return VK_LBUTTON;
    case MouseButton::Right:
        return VK_RBUTTON;
    case MouseButton::Middle:
        return VK_MBUTTON;
    case MouseButton::X1:
        return VK_XBUTTON1;
    case MouseButton::X2:
        return VK_XBUTTON2;
    }
    return VK_LBUTTON;
}

}  // namespace

Size screenSize()
{
    return Size{GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
}

bool isButtonPressed(KeyCode key)
{
    return GetAsyncKeyState(virtualKey(key)) < 0;
}

bool isButtonPressed(MouseButton button)
{
    return GetAsyncKeyState(mouseVirtualKey(button)) < 0;
}

}  // namespace skyinput
