#include "services/settings/shortcut_parser.hpp"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/translator.hpp"
#include "core/string_utils.hpp"

namespace sky {

namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 32> kTokenNames = {{
    {"up", "up"},
    {"down", "down"},
    {"left", "left"},
    {"right", "right"},
    {"numadd", "num+"},
    {"numsub", "num-"},
    {"nummult", "num*"},
    {"numdiv", "num/"},
    {"numdec", "num."},
    {"numcomma", "num,"},
    {"space", "space"},
    {"tab", "tab"},
    {"enter", "enter"},
    {"return", "enter"},
    {"backspace", "backspace"},
    {"escape", "escape"},
    {"esc", "escape"},
    {"delete", "delete"},
    {"del", "delete"},
    {"insert", "insert"},
    {"ins", "insert"},
    {"home", "home"},
    {"end", "end"},
    {"pageup", "pageUp"},
    {"pagedown", "pageDown"},
    {"capslock", "capsLock"},
    {"numlock", "numLock"},
    {"scrolllock", "scrollLock"},
    {"printscreen", "printScreen"},
    {"pause", "pause"},
    {"plus", "="},
    {"+", "="},
}};

std::optional<std::string> keyNameForToken(const std::string &lowered)
{
    for (const auto &[token, name] : kTokenNames) {
        if (token == lowered) {
            return std::string(name);
        }
    }
    if (lowered.size() == 4 && startsWith(lowered, "num") && lowered[3] >= '0' && lowered[3] <= '9') {
        return lowered;
    }
    if (lowered.size() >= 2 && lowered.size() <= 3 && lowered[0] == 'f') {
        return lowered;
    }
    if (lowered.size() == 1) {
        return lowered;
    }
    return std::nullopt;
}

Error invalidShortcut(std::string_view accelerator)
{
    return Error{"SHORTCUT_INVALID",
                 fromQString(tr("Unable to register the shortcut %1.").arg(toQString(std::string(accelerator)))), {}};
}

std::vector<std::string> splitTokens(std::string_view accelerator)
{
    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (start <= accelerator.size()) {
        const std::size_t plus = accelerator.find('+', start);
        const std::size_t end = plus == std::string_view::npos ? accelerator.size() : plus;
        if (end == start && plus != std::string_view::npos && end + 1 == accelerator.size()) {
            tokens.emplace_back("+");
            break;
        }
        tokens.emplace_back(trimJs(accelerator.substr(start, end - start)));
        if (plus == std::string_view::npos) {
            break;
        }
        start = plus + 1;
    }
    return tokens;
}

}  // namespace

Result<ParsedShortcut> parseShortcut(std::string_view accelerator)
{
    const std::string_view trimmed = trimJs(accelerator);
    if (trimmed.empty()) {
        return invalidShortcut(accelerator);
    }

    ParsedShortcut parsed;
    std::optional<skyinput::KeyCode> key;
    for (const std::string &token : splitTokens(trimmed)) {
        const std::string lowered = toLowerAscii(token);
        if (lowered == "ctrl" || lowered == "control" || lowered == "commandorcontrol" || lowered == "cmdorctrl") {
            parsed.modifiers.control = true;
            continue;
        }
        if (lowered == "alt" || lowered == "option" || lowered == "altgr") {
            parsed.modifiers.alt = true;
            continue;
        }
        if (lowered == "shift") {
            parsed.modifiers.shift = true;
            continue;
        }
        if (lowered == "win" || lowered == "super" || lowered == "meta" || lowered == "cmd" || lowered == "command") {
            parsed.modifiers.win = true;
            continue;
        }
        if (key) {
            return invalidShortcut(accelerator);
        }
        const std::optional<std::string> name = keyNameForToken(lowered);
        const std::optional<skyinput::KeyCode> code = name ? skyinput::keyFromName(*name) : std::nullopt;
        if (!code) {
            return invalidShortcut(accelerator);
        }
        key = code;
    }
    if (!key) {
        return invalidShortcut(accelerator);
    }
    parsed.key = *key;
    return parsed;
}

}  // namespace sky
