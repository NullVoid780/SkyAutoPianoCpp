#pragma once

#include <string_view>

#include "core/json.hpp"

namespace sky {

bool looksLikeAbc(std::string_view text);
OrderedJson parseAbcSheet(std::string_view text, std::string_view name);

}  // namespace sky
