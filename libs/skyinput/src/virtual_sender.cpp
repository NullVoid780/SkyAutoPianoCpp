#include "skyinput/virtual_sender.hpp"

#include <windows.h>

namespace skyinput {

namespace {

UINT buttonMessage(MouseButton button, bool down)
{
    switch (button) {
    case MouseButton::Left:
        return down ? WM_LBUTTONDOWN : WM_LBUTTONUP;
    case MouseButton::Right:
        return down ? WM_RBUTTONDOWN : WM_RBUTTONUP;
    case MouseButton::Middle:
        return down ? WM_MBUTTONDOWN : WM_MBUTTONUP;
    case MouseButton::X1:
    case MouseButton::X2:
        return down ? WM_XBUTTONDOWN : WM_XBUTTONUP;
    }
    return WM_LBUTTONUP;
}

WPARAM buttonWParam(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:
        return MK_LBUTTON;
    case MouseButton::Right:
        return MK_RBUTTON;
    case MouseButton::Middle:
        return MK_MBUTTON;
    case MouseButton::X1:
        return MK_XBUTTON1;
    case MouseButton::X2:
        return MK_XBUTTON2;
    }
    return 0;
}

Result<void> post(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!PostMessageW(window, message, wParam, lParam)) {
        return Error::fromLastError("PostMessageW");
    }
    return {};
}

}  // namespace

VirtualSender::VirtualSender(WindowHandle target) : m_target(target) {}

Result<void> VirtualSender::toggleKey(KeyCode key, bool down)
{
    const UINT vk = virtualKey(key);
    const UINT up = down ? 0u : 1u;
    const LPARAM lParam = static_cast<LPARAM>((vk << 16) | (up << 30) | (up << 31));
    return post(m_target.load(), down ? WM_KEYDOWN : WM_KEYUP, vk, lParam);
}

Result<void> VirtualSender::printChars(std::wstring_view text)
{
    for (wchar_t unit : text) {
        SendMessageW(m_target.load(), WM_CHAR, unit, 0);
    }
    return {};
}

Result<void> VirtualSender::toggleMouse(MouseButton button, bool down)
{
    const Point coords = lastCoords();
    return post(m_target.load(), buttonMessage(button, down), down ? buttonWParam(button) : 0,
                MAKELPARAM(coords.x, coords.y));
}

Result<void> VirtualSender::move(Point point, bool absolute)
{
    Point next;
    {
        std::lock_guard lock(m_coordsMutex);
        next = absolute ? point : Point{m_lastCoords.x + point.x, m_lastCoords.y + point.y};
        m_lastCoords = next;
    }
    return post(m_target.load(), WM_MOUSEMOVE, 1, MAKELPARAM(next.x, next.y));
}

Result<void> VirtualSender::scrollWheel(int amount)
{
    const Point coords = lastCoords();
    SendMessageW(m_target.load(), WM_MOUSEWHEEL, MAKEWPARAM(0, amount * WHEEL_DELTA),
                 MAKELPARAM(coords.x, coords.y));
    return {};
}

Point VirtualSender::cursorPosition() const
{
    POINT coords{};
    GetCursorPos(&coords);
    ScreenToClient(m_target.load(), &coords);
    return Point{coords.x, coords.y};
}

Point VirtualSender::lastCoords() const
{
    std::lock_guard lock(m_coordsMutex);
    return m_lastCoords;
}

void VirtualSender::setTarget(WindowHandle window)
{
    m_target.store(window);
}

WindowHandle VirtualSender::target() const
{
    return m_target.load();
}

}  // namespace skyinput
