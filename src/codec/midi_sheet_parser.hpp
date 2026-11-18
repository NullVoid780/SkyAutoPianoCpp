#pragma once

#include <cstddef>
#include <span>
#include <string_view>

#include "core/json.hpp"
#include "core/result.hpp"

namespace sky {

Result<OrderedJson> parseMidiSheet(std::span<const std::byte> content, std::string_view name);

}  // namespace sky
