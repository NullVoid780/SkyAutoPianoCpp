#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace sky {

std::string base64Encode(std::string_view bytes);
std::optional<std::string> base64Decode(std::string_view text);

}  // namespace sky
