#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace sky {

struct VersionNotes {
    std::string version;
    std::string markdown;
};

std::vector<VersionNotes> parseChangelog(std::string_view changelog);
std::vector<VersionNotes> missedVersions(const std::vector<VersionNotes> &changelog, std::string_view current,
                                         std::string_view latest);
const VersionNotes *findVersionNotes(const std::vector<VersionNotes> &changelog, std::string_view version);
std::string composeUpdateNotes(std::string_view latestNotes, const std::vector<VersionNotes> &missed);

}  // namespace sky
