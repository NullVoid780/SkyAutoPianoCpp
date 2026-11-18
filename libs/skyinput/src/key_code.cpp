#include "skyinput/key_code.hpp"

#include <windows.h>

#include <array>
#include <utility>

namespace skyinput {

namespace {

struct NamedKey {
    std::string_view name;
    std::uint16_t virtualKey;
};

constexpr std::array kKeyTable = {
    NamedKey{"backspace", VK_BACK},     NamedKey{"tab", VK_TAB},
    NamedKey{"enter", VK_RETURN},       NamedKey{"shift", VK_SHIFT},
    NamedKey{"ctrl", VK_CONTROL},       NamedKey{"alt", VK_MENU},
    NamedKey{"pause", VK_PAUSE},        NamedKey{"capsLock", VK_CAPITAL},
    NamedKey{"escape", VK_ESCAPE},      NamedKey{"space", VK_SPACE},
    NamedKey{"pageUp", VK_PRIOR},       NamedKey{"pageDown", VK_NEXT},
    NamedKey{"end", VK_END},            NamedKey{"home", VK_HOME},
    NamedKey{"left", VK_LEFT},          NamedKey{"up", VK_UP},
    NamedKey{"right", VK_RIGHT},        NamedKey{"down", VK_DOWN},
    NamedKey{"printScreen", VK_SNAPSHOT}, NamedKey{"insert", VK_INSERT},
    NamedKey{"delete", VK_DELETE},      NamedKey{"0", 0x30},
    NamedKey{"1", 0x31},                NamedKey{"2", 0x32},
    NamedKey{"3", 0x33},                NamedKey{"4", 0x34},
    NamedKey{"5", 0x35},                NamedKey{"6", 0x36},
    NamedKey{"7", 0x37},                NamedKey{"8", 0x38},
    NamedKey{"9", 0x39},                NamedKey{"a", 0x41},
    NamedKey{"b", 0x42},                NamedKey{"c", 0x43},
    NamedKey{"d", 0x44},                NamedKey{"e", 0x45},
    NamedKey{"f", 0x46},                NamedKey{"g", 0x47},
    NamedKey{"h", 0x48},                NamedKey{"i", 0x49},
    NamedKey{"j", 0x4a},                NamedKey{"k", 0x4b},
    NamedKey{"l", 0x4c},                NamedKey{"m", 0x4d},
    NamedKey{"n", 0x4e},                NamedKey{"o", 0x4f},
    NamedKey{"p", 0x50},                NamedKey{"q", 0x51},
    NamedKey{"r", 0x52},                NamedKey{"s", 0x53},
    NamedKey{"t", 0x54},                NamedKey{"u", 0x55},
    NamedKey{"v", 0x56},                NamedKey{"w", 0x57},
    NamedKey{"x", 0x58},                NamedKey{"y", 0x59},
    NamedKey{"z", 0x5a},                NamedKey{"lWin", VK_LWIN},
    NamedKey{"rWin", VK_RWIN},          NamedKey{"num0", VK_NUMPAD0},
    NamedKey{"num1", VK_NUMPAD1},       NamedKey{"num2", VK_NUMPAD2},
    NamedKey{"num3", VK_NUMPAD3},       NamedKey{"num4", VK_NUMPAD4},
    NamedKey{"num5", VK_NUMPAD5},       NamedKey{"num6", VK_NUMPAD6},
    NamedKey{"num7", VK_NUMPAD7},       NamedKey{"num8", VK_NUMPAD8},
    NamedKey{"num9", VK_NUMPAD9},       NamedKey{"num*", VK_MULTIPLY},
    NamedKey{"num+", VK_ADD},           NamedKey{"num,", VK_SEPARATOR},
    NamedKey{"num-", VK_SUBTRACT},      NamedKey{"num.", VK_DECIMAL},
    NamedKey{"num/", VK_DIVIDE},        NamedKey{"f1", VK_F1},
    NamedKey{"f2", VK_F2},              NamedKey{"f3", VK_F3},
    NamedKey{"f4", VK_F4},              NamedKey{"f5", VK_F5},
    NamedKey{"f6", VK_F6},              NamedKey{"f7", VK_F7},
    NamedKey{"f8", VK_F8},              NamedKey{"f9", VK_F9},
    NamedKey{"f10", VK_F10},            NamedKey{"f11", VK_F11},
    NamedKey{"f12", VK_F12},            NamedKey{"f13", VK_F13},
    NamedKey{"f14", VK_F14},            NamedKey{"f15", VK_F15},
    NamedKey{"f16", VK_F16},            NamedKey{"f17", VK_F17},
    NamedKey{"f18", VK_F18},            NamedKey{"f19", VK_F19},
    NamedKey{"f20", VK_F20},            NamedKey{"f21", VK_F21},
    NamedKey{"f22", VK_F22},            NamedKey{"f23", VK_F23},
    NamedKey{"f24", VK_F24},            NamedKey{"numLock", VK_NUMLOCK},
    NamedKey{"scrollLock", VK_SCROLL},  NamedKey{"lShift", VK_LSHIFT},
    NamedKey{"rShift", VK_RSHIFT},      NamedKey{"lCtrl", VK_LCONTROL},
    NamedKey{"rCtrl", VK_RCONTROL},     NamedKey{"lAlt", VK_LMENU},
    NamedKey{"rAlt", VK_RMENU},         NamedKey{";", VK_OEM_1},
    NamedKey{"=", VK_OEM_PLUS},         NamedKey{",", VK_OEM_COMMA},
    NamedKey{"-", VK_OEM_MINUS},        NamedKey{".", VK_OEM_PERIOD},
    NamedKey{"/", VK_OEM_2},            NamedKey{"`", VK_OEM_3},
    NamedKey{"[", VK_OEM_4},            NamedKey{"\\", VK_OEM_5},
    NamedKey{"]", VK_OEM_6},            NamedKey{"'", VK_OEM_7},
};

constexpr auto kNames = [] {
    std::array<std::string_view, kKeyTable.size()> names{};
    for (std::size_t i = 0; i < kKeyTable.size(); ++i) {
        names[i] = kKeyTable[i].name;
    }
    return names;
}();

}  // namespace

std::optional<KeyCode> keyFromName(std::string_view name)
{
    for (const NamedKey &entry : kKeyTable) {
        if (entry.name == name) {
            return keyCodeFromVirtualKey(entry.virtualKey);
        }
    }
    return std::nullopt;
}

std::string_view nameFromKey(KeyCode key)
{
    std::string_view best;
    for (const NamedKey &entry : kKeyTable) {
        if (entry.virtualKey == virtualKey(key) && (best.empty() || entry.name < best)) {
            best = entry.name;
        }
    }
    return best;
}

std::span<const std::string_view> keyNames()
{
    return kNames;
}

}  // namespace skyinput
