#include "services/update/github_release.hpp"

#include <algorithm>
#include <cctype>

#include "core/string_utils.hpp"
#include "services/update/version.hpp"

namespace sky {

namespace {

constexpr std::string_view kCriticalMarker = "critical";
constexpr std::string_view kFullChangelog = "**Full Changelog**";
constexpr std::string_view kTagPath = "/releases/tag/";
constexpr std::string_view kDigestPrefix = "sha256:";
constexpr std::string_view kAssetPrefix = "skyautopiano";
constexpr std::string_view kFooterRule = "---";

bool endsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

bool isSha256Hex(std::string_view text)
{
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](char c) {
               return std::isxdigit(static_cast<unsigned char>(c)) != 0;
           });
}

std::string stringMember(const Json &json, const char *name)
{
    const auto found = json.find(name);
    return found != json.end() && found->is_string() ? found->get<std::string>() : std::string();
}

std::string trimmed(std::string_view text)
{
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(first, last - first + 1));
}

std::optional<UpdatePackage> packageFrom(const Json &assets)
{
    if (!assets.is_array()) {
        return std::nullopt;
    }
    std::optional<UpdatePackage> zip;
    for (const Json &asset : assets) {
        if (!asset.is_object()) {
            continue;
        }
        const std::string name = toLowerAscii(stringMember(asset, "name"));
        const bool installer = endsWith(name, "-setup.exe");
        if (name.rfind(kAssetPrefix, 0) != 0 || (!installer && !endsWith(name, ".zip"))) {
            continue;
        }
        UpdatePackage package;
        package.url = stringMember(asset, "browser_download_url");
        if (package.url.rfind("https://", 0) != 0) {
            continue;
        }
        package.kind = installer ? PackageKind::Installer : PackageKind::Zip;
        if (const auto size = asset.find("size"); size != asset.end() && size->is_number_unsigned()) {
            package.size = size->get<std::uint64_t>();
        }
        const std::string digest = toLowerAscii(stringMember(asset, "digest"));
        if (digest.rfind(kDigestPrefix, 0) == 0 && isSha256Hex(std::string_view(digest).substr(kDigestPrefix.size()))) {
            package.sha256 = digest.substr(kDigestPrefix.size());
        }
        if (installer) {
            return package;
        }
        zip = package;
    }
    return zip;
}

}  // namespace

std::string installerFileName(std::string_view version)
{
    return "SkyAutoPiano-" + std::string(version) + "-setup.exe";
}

std::string cleanReleaseNotes(std::string_view body, bool *critical)
{
    std::string text;
    text.reserve(body.size());
    bool marked = false;
    std::size_t pos = 0;
    while (pos < body.size()) {
        const std::size_t open = body.find("<!--", pos);
        if (open == std::string_view::npos) {
            text.append(body.substr(pos));
            break;
        }
        text.append(body.substr(pos, open - pos));
        const std::size_t close = body.find("-->", open + 4);
        const std::size_t end = close == std::string_view::npos ? body.size() : close;
        if (toLowerAscii(trimmed(body.substr(open + 4, end - open - 4))) == kCriticalMarker) {
            marked = true;
        }
        pos = close == std::string_view::npos ? body.size() : close + 3;
    }

    std::string notes;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t stop = text.find('\n', start);
        if (stop == std::string::npos) {
            stop = text.size();
        }
        std::string line = text.substr(start, stop - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (trimmed(line) == kFooterRule) {
            break;
        }
        if (trimmed(line).rfind(kFullChangelog, 0) != 0) {
            notes += line;
            notes += '\n';
        }
        start = stop + 1;
    }
    if (critical) {
        *critical = marked;
    }
    return trimmed(notes);
}

std::optional<ReleaseInfo> parseRelease(const Json &release)
{
    if (!release.is_object() || release.value("draft", false) || release.value("prerelease", false)) {
        return std::nullopt;
    }
    ReleaseInfo info;
    info.version = versionFromTag(stringMember(release, "tag_name"));
    if (info.version.empty() || !std::isdigit(static_cast<unsigned char>(info.version.front()))) {
        return std::nullopt;
    }
    info.notes = cleanReleaseNotes(stringMember(release, "body"), &info.critical);
    info.page = stringMember(release, "html_url");
    if (const auto assets = release.find("assets"); assets != release.end()) {
        info.package = packageFrom(*assets);
    }
    return info;
}

std::vector<ReleaseInfo> parseReleases(const Json &releases)
{
    std::vector<ReleaseInfo> parsed;
    if (!releases.is_array()) {
        return parsed;
    }
    for (const Json &release : releases) {
        if (auto info = parseRelease(release)) {
            parsed.push_back(std::move(*info));
        }
    }
    std::stable_sort(parsed.begin(), parsed.end(), [](const ReleaseInfo &a, const ReleaseInfo &b) {
        return compareVersions(a.version, b.version) > 0;
    });
    return parsed;
}

std::string versionFromReleaseUrl(std::string_view url)
{
    const std::size_t tag = url.find(kTagPath);
    if (tag == std::string_view::npos) {
        return {};
    }
    std::string_view rest = url.substr(tag + kTagPath.size());
    rest = rest.substr(0, rest.find_first_of("/?#"));
    const std::string version = versionFromTag(rest);
    return !version.empty() && std::isdigit(static_cast<unsigned char>(version.front())) ? version : std::string();
}

}  // namespace sky
