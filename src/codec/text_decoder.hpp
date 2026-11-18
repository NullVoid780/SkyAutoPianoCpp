#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace sky {

std::string decodeSheetText(std::span<const std::byte> content);

}  // namespace sky
