#pragma once

#include <array>
#include <string>

namespace sky {

enum class PlaybackMode { Lite, Strict };
enum class AppTheme { Light, Dark };

struct PanelConfig {
    bool longPressMode = false;
    double speed = 1.0;
    double delayNext = 1.0;
    bool autoSave = true;
    bool minimizeOnPlay = true;
    PlaybackMode playbackMode = PlaybackMode::Lite;
};

struct KeyboardConfig {
    bool customKeyboard = false;
    std::array<std::string, 15> keys = {"y", "u", "i", "o", "p", "h", "j", "k",
                                        "l", ";", "n", "m", ",", ".", "/"};
};

struct ShortcutConfig {
    std::string pre = "Ctrl+Shift+C";
    std::string play = "Ctrl+Shift+V";
    std::string next = "Ctrl+Shift+B";
    std::string increaseSpeed = "Ctrl+Up";
    std::string decreaseSpeed = "Ctrl+Down";
};

struct UpdateConfig {
    bool blockUpdate = false;
};

struct Config {
    PanelConfig panel;
    KeyboardConfig keyboard;
    ShortcutConfig shortcut;
    UpdateConfig update;
    AppTheme appTheme = AppTheme::Dark;
};

}  // namespace sky
