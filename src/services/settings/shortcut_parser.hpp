#pragma once

#include <string_view>

#include "core/result.hpp"
#include "skyinput/global_hotkey.hpp"
#include "skyinput/key_code.hpp"

namespace sky {

struct ParsedShortcut {
    skyinput::KeyCode key{};
    skyinput::Modifiers modifiers;
};

Result<ParsedShortcut> parseShortcut(std::string_view accelerator);

}  // namespace sky
