#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/json.hpp"
#include "core/result.hpp"

namespace sky {

inline constexpr int kReportSchema = 1;
inline constexpr const char *kIssueReportExtension = ".sapr";
inline constexpr const char *kCrashReportExtension = ".sapcr";

struct ReportPackage {
    OrderedJson manifest = OrderedJson::object();
    std::vector<std::pair<std::string, std::filesystem::path>> files;
    std::vector<std::pair<std::string, std::string>> blobs;
};

Result<void> writeReportPackage(const std::filesystem::path &target, const ReportPackage &package);
std::string readFileTail(const std::filesystem::path &file, std::size_t maxBytes);
std::optional<std::filesystem::path> latestCrashReport(const std::filesystem::path &directory);
std::optional<std::string> readArchiveEntry(const std::filesystem::path &archive, const std::string &name);
std::string safeArchiveName(const std::string &name);

}  // namespace sky
