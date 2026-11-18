#pragma once

#include <filesystem>
#include <string>

#include "core/result.hpp"

namespace sky {

std::string decodeTextBytes(std::string_view bytes);
Result<std::string> readTextFile(const std::filesystem::path &file);

}  // namespace sky
