#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/json.hpp"

namespace sky {

enum class PackageKind { Installer, Zip };

struct UpdatePackage {
    std::string url;
    PackageKind kind = PackageKind::Installer;
    std::uint64_t size = 0;
    std::string sha256;
};

struct ReleaseInfo {
    std::string version;
    std::string notes;
    bool critical = false;
    std::string page;
    std::optional<UpdatePackage> package;
};

std::string installerFileName(std::string_view version);
std::string cleanReleaseNotes(std::string_view body, bool *critical = nullptr);
std::optional<ReleaseInfo> parseRelease(const Json &release);
std::vector<ReleaseInfo> parseReleases(const Json &releases);
std::string versionFromReleaseUrl(std::string_view url);

}  // namespace sky
