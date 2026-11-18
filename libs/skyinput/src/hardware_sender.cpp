#include "skyinput/hardware_sender.hpp"

#include <windows.h>

#include <vector>

namespace skyinput {

namespace {

Result<void> send(INPUT *inputs, UINT count)
{
    if (count == 0) {
        return {};
    }
    if (SendInput(count, inputs, sizeof(INPUT)) != count) {
        return Error::fromLastError("SendInput");
    }
    return {};
}

DWORD mouseFlags(MouseButton button, bool down)
{
    switch (button) {
    case MouseButton::Left:
        return down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    case MouseButton::Right:
        return down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    case MouseButton::Middle:
        return down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    case MouseButton::X1:
    case MouseButton::X2:
        return down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    }
    return 0;
}

}  // namespace

HardwareSender::HardwareSender(WindowHandle target) : m_target(target) {}

Result<void> HardwareSender::toggleKey(KeyCode key, bool down)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0;
    input.ki.wScan = scanCodeOf(key);
    input.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP) |
                       (isExtendedKey(key) ? KEYEVENTF_EXTENDEDKEY : 0);
    return send(&input, 1);
}

Result<void> HardwareSender::printChars(std::wstring_view text)
{
    std::vector<INPUT> inputs;
    inputs.reserve(text.size() * 2);
    for (wchar_t unit : text) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = unit;
        input.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(input);
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
        inputs.push_back(input);
    }
    return send(inputs.data(), static_cast<UINT>(inputs.size()));
}

Result<void> HardwareSender::toggleMouse(MouseButton button, bool down)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.mouseData = button == MouseButton::X1 ? XBUTTON1 : button == MouseButton::X2 ? XBUTTON2 : 0;
    input.mi.dwFlags = mouseFlags(button, down);
    return send(&input, 1);
}

Result<void> HardwareSender::move(Point point, bool absolute)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    if (absolute) {
        POINT coords{point.x, point.y};
        ClientToScreen(m_target.load(), &coords);
        const int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (coords.x < 0) {
            coords.x = 0;
        } else if (coords.x >= screenWidth) {
            coords.x = screenWidth - 1;
        }
        if (coords.y < 0) {
            coords.y = 0;
        } else if (coords.y >= screenHeight) {
            coords.y = screenHeight - 1;
        }
        input.mi.dx = ((coords.x - GetSystemMetrics(SM_XVIRTUALSCREEN) + 1) << 16) / screenWidth;
        input.mi.dy = ((coords.y - GetSystemMetrics(SM_YVIRTUALSCREEN) + 1) << 16) / screenHeight;
        input.mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    } else {
        input.mi.dx = point.x;
        input.mi.dy = point.y;
    }
    return send(&input, 1);
}

Result<void> HardwareSender::scrollWheel(int amount)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.mouseData = static_cast<DWORD>(amount * WHEEL_DELTA);
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    return send(&input, 1);
}

Point HardwareSender::cursorPosition() const
{
    POINT coords{};
    GetCursorPos(&coords);
    ScreenToClient(m_target.load(), &coords);
    return Point{coords.x, coords.y};
}

Point HardwareSender::lastCoords() const
{
    return cursorPosition();
}

void HardwareSender::setTarget(WindowHandle window)
{
    m_target.store(window);
}

WindowHandle HardwareSender::target() const
{
    return m_target.load();
}

}  // namespace skyinput
