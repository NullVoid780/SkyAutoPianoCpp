#pragma once

#include <string>
#include <string_view>

namespace sky {

int compareVersions(std::string_view left, std::string_view right);
std::string versionFromTag(std::string_view tag);

}  // namespace sky
