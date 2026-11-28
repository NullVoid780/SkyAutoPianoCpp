#include "services/update/release_notes.hpp"

#include <algorithm>

#include "core/translator.hpp"
#include "core/string_utils.hpp"
#include "services/update/version.hpp"

namespace sky {

namespace {

std::vector<std::string_view> splitLines(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        std::string_view line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        lines.push_back(line);
        start = end + 1;
    }
    return lines;
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

}  // namespace

std::vector<VersionNotes> parseChangelog(std::string_view changelog)
{
    std::vector<VersionNotes> versions;
    std::string version;
    std::string body;
    const auto flush = [&] {
        if (!version.empty()) {
            versions.push_back(VersionNotes{version, trimmed(body)});
        }
        body.clear();
    };
    for (std::string_view line : splitLines(changelog)) {
        if (line.rfind("## ", 0) == 0) {
            flush();
            version = versionFromTag(line.substr(3));
            continue;
        }
        if (!version.empty()) {
            body.append(line);
            body.push_back('\n');
        }
    }
    flush();
    return versions;
}

std::vector<VersionNotes> missedVersions(const std::vector<VersionNotes> &changelog, std::string_view current,
                                         std::string_view latest)
{
    std::vector<VersionNotes> missed;
    for (const VersionNotes &entry : changelog) {
        if (compareVersions(entry.version, current) > 0 && compareVersions(entry.version, latest) < 0 &&
            !entry.markdown.empty()) {
            missed.push_back(entry);
        }
    }
    std::sort(missed.begin(), missed.end(), [](const VersionNotes &a, const VersionNotes &b) {
        return compareVersions(a.version, b.version) > 0;
    });
    return missed;
}

const VersionNotes *findVersionNotes(const std::vector<VersionNotes> &changelog, std::string_view version)
{
    const auto found = std::find_if(changelog.begin(), changelog.end(), [version](const VersionNotes &entry) {
        return compareVersions(entry.version, version) == 0;
    });
    return found != changelog.end() ? &*found : nullptr;
}

std::string composeUpdateNotes(std::string_view latestNotes, const std::vector<VersionNotes> &missed)
{
    std::string markdown = "## " + trUtf8("What's new?") + "\n\n";
    const std::string notes = trimmed(latestNotes);
    markdown += notes.empty() ? trUtf8("No release notes for this version.") : notes;
    if (!missed.empty()) {
        markdown += "\n\n---\n\n## " + trUtf8("Other updates you've missed") + "\n";
        for (const VersionNotes &entry : missed) {
            markdown += "\n### " + fromQString(tr("Version %1").arg(toQString(entry.version))) + "\n\n" +
                        entry.markdown + "\n";
        }
    }
    return markdown;
}

}  // namespace sky
