#pragma once

#include <filesystem>
#include <vector>

#include "core/result.hpp"

namespace sky {

Result<std::vector<std::filesystem::path>> extractZip(const std::filesystem::path &archive,
                                                      const std::filesystem::path &destination);

}  // namespace sky
