#pragma once

#include <algorithm>
#include <array>

#include <QColor>

#include "ui/theme_manager.hpp"

namespace sky {

inline constexpr int kKeysPerPadRow = 5;

inline QColor keyAccentColor()
{
    return QColor(0x3B, 0x82, 0xF6);
}

inline bool &colorfulNotesSetting()
{
    static bool colorful = true;
    return colorful;
}

inline bool colorfulNotes()
{
    return colorfulNotesSetting();
}

inline void setColorfulNotes(bool colorful)
{
    colorfulNotesSetting() = colorful;
}

inline bool darkSurface()
{
    return currentTheme().background.lightnessF() < 0.5;
}

inline QColor keyNoteColor(int key, bool colorful)
{
    if (!colorful) {
        return darkSurface() ? QColor(0xE5, 0xE7, 0xEB) : QColor(0x33, 0x41, 0x55);
    }
    static const std::array<QColor, 3> colors = {QColor(0x3B, 0x82, 0xF6), QColor(0x16, 0xA3, 0x4A),
                                                 QColor(0x93, 0x5B, 0xE8)};
    return colors[static_cast<std::size_t>(std::clamp(key / kKeysPerPadRow, 0, 2))];
}

inline QColor keyRowColor(int key)
{
    return keyNoteColor(key, colorfulNotes());
}

inline QColor keyInkColor(const QColor &fill)
{
    return fill.lightnessF() > 0.62 ? QColor(0x1F, 0x29, 0x37) : QColor(Qt::white);
}

}  // namespace sky
