#include "services/update/version.hpp"

#include <algorithm>
#include <vector>

#include "core/string_utils.hpp"

namespace sky {

namespace {

std::vector<long long> versionParts(std::string_view version)
{
    version = trimJs(version);
    if (!version.empty() && (version.front() == 'v' || version.front() == 'V')) {
        version.remove_prefix(1);
    }
    const std::size_t suffix = version.find_first_of("-+");
    if (suffix != std::string_view::npos) {
        version = version.substr(0, suffix);
    }
    std::vector<long long> parts;
    long long current = 0;
    bool digits = false;
    for (char c : version) {
        if (c >= '0' && c <= '9') {
            current = current * 10 + (c - '0');
            digits = true;
        } else if (c == '.') {
            parts.push_back(digits ? current : 0);
            current = 0;
            digits = false;
        } else {
            break;
        }
    }
    parts.push_back(digits ? current : 0);
    return parts;
}

long long part(const std::vector<long long> &parts, std::size_t index)
{
    return index < parts.size() ? parts[index] : 0;
}

}  // namespace

int compareVersions(std::string_view left, std::string_view right)
{
    const std::vector<long long> a = versionParts(left);
    const std::vector<long long> b = versionParts(right);
    for (std::size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
        if (part(a, i) != part(b, i)) {
            return part(a, i) < part(b, i) ? -1 : 1;
        }
    }
    return 0;
}

std::string versionFromTag(std::string_view tag)
{
    tag = trimJs(tag);
    if (!tag.empty() && (tag.front() == 'v' || tag.front() == 'V')) {
        tag.remove_prefix(1);
    }
    return std::string(tag);
}

}  // namespace sky
