#include "platform/window_attributes.hpp"

#include <windows.h>
#include <dwmapi.h>

#include <QColor>
#include <QWidget>

namespace sky {

namespace {

constexpr DWORD kImmersiveDarkMode = 20;
constexpr DWORD kBorderColor = 34;
constexpr DWORD kCaptionColor = 35;
constexpr DWORD kTextColor = 36;

COLORREF toColorRef(const QColor &color)
{
    return RGB(color.red(), color.green(), color.blue());
}

}  // namespace

void applyTitleBarColors(QWidget *window, const QColor &fill, const QColor &text, const QColor &border)
{
    if (!window) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    const BOOL dark = fill.lightnessF() < 0.5f ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kImmersiveDarkMode, &dark, sizeof(dark));
    const COLORREF caption = toColorRef(fill);
    const COLORREF title = toColorRef(text);
    const COLORREF edge = toColorRef(border);
    DwmSetWindowAttribute(hwnd, kCaptionColor, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, kTextColor, &title, sizeof(title));
    DwmSetWindowAttribute(hwnd, kBorderColor, &edge, sizeof(edge));
}

}  // namespace sky
