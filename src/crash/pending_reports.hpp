#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace sky {

inline constexpr std::size_t kKeptCrashReports = 10;
inline constexpr std::uintmax_t kMaxDumpBytes = 256ull * 1024 * 1024;

std::filesystem::path crashDirectory();
std::string newReportId();
void pruneCrashReports(const std::filesystem::path &directory, std::size_t keep = kKeptCrashReports,
                       std::uintmax_t maxDumpBytes = kMaxDumpBytes);

}  // namespace sky
