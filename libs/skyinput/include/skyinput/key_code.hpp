#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace skyinput {

enum class KeyCode : std::uint16_t {};

constexpr KeyCode keyCodeFromVirtualKey(std::uint16_t virtualKey)
{
    return static_cast<KeyCode>(virtualKey);
}

constexpr std::uint16_t virtualKey(KeyCode key)
{
    return static_cast<std::uint16_t>(key);
}

std::optional<KeyCode> keyFromName(std::string_view name);
std::string_view nameFromKey(KeyCode key);
std::span<const std::string_view> keyNames();

std::uint16_t scanCodeOf(KeyCode key);
bool isExtendedKey(KeyCode key);

}  // namespace skyinput
