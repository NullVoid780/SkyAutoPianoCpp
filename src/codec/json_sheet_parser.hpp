#pragma once

#include <string_view>

#include "core/json.hpp"
#include "core/result.hpp"

namespace sky {

Result<OrderedJson> parseJsonSheet(std::string_view text);

}  // namespace sky
