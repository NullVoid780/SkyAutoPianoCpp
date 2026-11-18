#pragma once

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "core/string_utils.hpp"
#include "model/config.hpp"

namespace sky {

inline constexpr int kSkyKeyCount = 15;

inline constexpr std::array<std::string_view, kSkyKeyCount> kAutoKeys = {
    "y", "u", "i", "o", "p", "h", "j", "k", "l", ";", "n", "m", ",", ".", "/"};

inline int autoKeyIndex(std::string_view key)
{
    for (int i = 0; i < kSkyKeyCount; ++i) {
        if (kAutoKeys[i] == key) {
            return i;
        }
    }
    return -1;
}

inline std::string songKeyName(int index)
{
    return "1Key" + std::to_string(index);
}


inline int autoKeyCollationRank(std::string_view key)
{
    static constexpr std::array<std::string_view, kSkyKeyCount> kCollationOrder = {
        ",", ";", ".", "/", "h", "i", "j", "k", "l", "m", "n", "o", "p", "u", "y"};
    for (int i = 0; i < kSkyKeyCount; ++i) {
        if (kCollationOrder[i] == key) {
            return i;
        }
    }
    return kSkyKeyCount;
}

inline bool autoKeyLess(std::string_view a, std::string_view b)
{
    const int rankA = autoKeyCollationRank(a);
    const int rankB = autoKeyCollationRank(b);
    if (rankA != rankB) {
        return rankA < rankB;
    }
    return a < b;
}

inline std::string_view legacyKeyAlias(std::string_view lowered)
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 8> kAliases = {{
        {"esc", "escape"},
        {"pageup", "pageUp"},
        {"pagedown", "pageDown"},
        {"numadd", "num+"},
        {"numsub", "num-"},
        {"nummult", "num*"},
        {"numdiv", "num/"},
        {"numdec", "num."},
    }};
    for (const auto &[alias, canonical] : kAliases) {
        if (alias == lowered) {
            return canonical;
        }
    }
    return {};
}

inline std::string normalizeOutputKeyName(std::string_view key)
{
    const std::string trimmed(trimJs(key));
    const std::string_view alias = legacyKeyAlias(toLowerAscii(trimmed));
    return alias.empty() ? trimmed : std::string(alias);
}

inline std::string normalizeKeyboardMappingValue(std::string_view value)
{
    static constexpr std::array<std::string_view, 29> kCanonical = {
        "space", "enter", "escape", "backspace", "delete", "insert", "home", "end", "pageUp", "pageDown",
        "up", "down", "left", "right", "num0", "num1", "num2", "num3", "num4", "num5",
        "num6", "num7", "num8", "num9", "num+", "num-", "num*", "num/", "num."};
    const std::string raw(trimJs(value));
    const std::string lowered = toLowerAscii(raw);
    if (lowered.empty()) {
        return {};
    }
    if (const std::string_view alias = legacyKeyAlias(lowered); !alias.empty()) {
        return std::string(alias);
    }
    for (std::string_view canonical : kCanonical) {
        if (toLowerAscii(canonical) == lowered) {
            return std::string(canonical);
        }
    }
    if (utf16Length(lowered) == 1) {
        return lowered;
    }
    return raw;
}

inline std::string resolveOutputKey(std::string_view rawKey, const KeyboardConfig &keyboard)
{
    if (!keyboard.customKeyboard) {
        return std::string(rawKey);
    }
    const int index = autoKeyIndex(rawKey);
    if (index < 0) {
        return std::string(rawKey);
    }
    const std::string &mapped = keyboard.keys[static_cast<std::size_t>(index)];
    return normalizeOutputKeyName(mapped.empty() ? rawKey : std::string_view(mapped));
}

}  // namespace sky
