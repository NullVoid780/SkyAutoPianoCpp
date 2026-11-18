#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "core/result.hpp"

namespace sky {

Result<void> ensureDir(const std::filesystem::path &dir);
Result<void> copyDir(const std::filesystem::path &from, const std::filesystem::path &to);
Result<void> removeDir(const std::filesystem::path &dir);
Result<void> safeDelete(const std::filesystem::path &file);
Result<void> atomicWrite(const std::filesystem::path &file, std::string_view bytes);
Result<std::vector<std::byte>> readFileBytes(const std::filesystem::path &file);
bool isInsideDirectory(const std::filesystem::path &candidate, const std::filesystem::path &root);

}  // namespace sky
