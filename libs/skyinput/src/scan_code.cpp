#include "skyinput/key_code.hpp"

#include <windows.h>

#include <algorithm>
#include <array>

namespace skyinput {

namespace {

constexpr std::array<std::uint16_t, 32> kExtendedKeys = {
    VK_RCONTROL,         VK_SNAPSHOT,         VK_RMENU,          VK_PAUSE,
    VK_HOME,             VK_UP,               VK_PRIOR,          VK_LEFT,
    VK_RIGHT,            VK_END,              VK_DOWN,           VK_NEXT,
    VK_INSERT,           VK_DELETE,           VK_LWIN,           VK_RWIN,
    VK_APPS,             VK_VOLUME_MUTE,      VK_VOLUME_DOWN,    VK_VOLUME_UP,
    VK_MEDIA_NEXT_TRACK, VK_MEDIA_PREV_TRACK, VK_MEDIA_STOP,     VK_MEDIA_PLAY_PAUSE,
    VK_BROWSER_BACK,     VK_BROWSER_FORWARD,  VK_BROWSER_REFRESH, VK_BROWSER_STOP,
    VK_BROWSER_SEARCH,   VK_BROWSER_FAVORITES, VK_BROWSER_HOME,  VK_LAUNCH_MAIL,
};

}  // namespace

std::uint16_t scanCodeOf(KeyCode key)
{
    return static_cast<std::uint16_t>(MapVirtualKeyW(virtualKey(key), MAPVK_VK_TO_VSC));
}

bool isExtendedKey(KeyCode key)
{
    return std::find(kExtendedKeys.begin(), kExtendedKeys.end(), virtualKey(key)) != kExtendedKeys.end();
}

}  // namespace skyinput
