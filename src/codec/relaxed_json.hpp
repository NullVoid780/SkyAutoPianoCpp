#pragma once

#include <string>
#include <string_view>

namespace sky {

std::string stripCallWrapper(std::string_view text);
std::string toRelaxedJson(std::string_view text);
std::string jsonStringLiteral(std::string_view text);

}  // namespace sky
