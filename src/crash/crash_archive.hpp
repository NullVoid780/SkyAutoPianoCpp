#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace sky {

inline constexpr const char *kCrashArchiveExtension = ".sapcr";

struct CrashArchiveInput {
    std::string id;
    nlohmann::ordered_json report;
    std::optional<std::filesystem::path> dump;
    std::filesystem::path appHome;
};

std::filesystem::path crashAppHome(const std::filesystem::path &executable);
bool writeCrashArchive(const std::filesystem::path &target, const CrashArchiveInput &input);

}  // namespace sky
