#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

const std::filesystem::path kRoot = std::filesystem::path(SKY_SOURCE_DIR);

std::string readText(const std::filesystem::path &file) {
    std::ifstream input(file, std::ios::binary);
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

std::vector<std::filesystem::path> filesUnder(const std::filesystem::path &folder,
                                              const std::set<std::string> &extensions) {
    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(folder)) {
        if (entry.is_regular_file() && extensions.contains(entry.path().extension().string())) {
            files.push_back(entry.path());
        }
    }
    return files;
}

std::string capture(const std::string &text, const std::string &pattern) {
    std::smatch match;
    return std::regex_search(text, match, std::regex(pattern)) ? match[1].str() : std::string();
}

std::string joined(const std::vector<std::string> &items) {
    std::string text;
    for (const std::string &item : items) {
        text += "\n  " + item;
    }
    return text;
}

std::vector<std::string> unlistedSources(const std::filesystem::path &folder,
                                         const std::filesystem::path &listFile) {
    const std::string list = readText(listFile);
    std::vector<std::string> missing;
    for (const auto &file : filesUnder(folder, {".cpp"})) {
        const std::string relative =
            std::filesystem::relative(file, listFile.parent_path()).generic_string();
        if (list.find(relative) == std::string::npos) {
            missing.push_back(relative);
        }
    }
    return missing;
}

}  // namespace

TEST_CASE("the app version is the same in CMakeLists.txt and vcpkg.json") {
    const std::string cmakeVersion = capture(readText(kRoot / "CMakeLists.txt"),
                                             R"re(set\(SKY_VERSION "([0-9]+\.[0-9]+\.[0-9]+)")re");
    const std::string vcpkgVersion = capture(
        readText(kRoot / "vcpkg.json"), R"re("version-semver"\s*:\s*"([0-9]+\.[0-9]+\.[0-9]+)")re");
    REQUIRE_FALSE(cmakeVersion.empty());
    CHECK(vcpkgVersion == cmakeVersion);
}

TEST_CASE("GitHub Actions use the same vcpkg version as vcpkg.json") {
    const std::string baseline =
        capture(readText(kRoot / "vcpkg.json"), R"re("builtin-baseline"\s*:\s*"([0-9a-f]{40})")re");
    const std::string ci = capture(readText(kRoot / ".github" / "actions" / "setup" / "action.yml"),
                                   R"re(vcpkgGitCommitId:\s*([0-9a-f]{40}))re");
    REQUIRE_FALSE(baseline.empty());
    CHECK(ci == baseline);
}

TEST_CASE("every source file is part of the build") {
    const auto app = unlistedSources(kRoot / "src", kRoot / "src" / "CMakeLists.txt");
    INFO("not listed in src/CMakeLists.txt:" << joined(app));
    CHECK(app.empty());
    const auto input = unlistedSources(kRoot / "libs" / "skyinput",
                                       kRoot / "libs" / "skyinput" / "CMakeLists.txt");
    INFO("not listed in libs/skyinput/CMakeLists.txt:" << joined(input));
    CHECK(input.empty());
}

TEST_CASE("every test file runs") {
    const auto missing = unlistedSources(kRoot / "tests", kRoot / "tests" / "CMakeLists.txt");
    INFO("not listed in tests/CMakeLists.txt, so these tests never run:" << joined(missing));
    CHECK(missing.empty());
}

TEST_CASE("every icon the code asks for exists") {
    std::set<std::string> icons;
    for (const auto &file : filesUnder(kRoot / "assets" / "icons", {".svg"})) {
        icons.insert(file.stem().string());
    }
    REQUIRE_FALSE(icons.empty());
    const std::vector<std::regex> patterns = {
        std::regex(R"re(IconCache::(?:icon|pixmap)\(\s*QStringLiteral\("([^"]+)"\))re"),
        std::regex(R"re(IconButton\(\s*QStringLiteral\("([^"]+)"\))re"),
        std::regex(R"re(ToolButton\(\s*QStringLiteral\("([^"]+)"\))re"),
        std::regex(
            R"re(ActionButton\(\s*ActionButton::Kind::\w+\s*,\s*QStringLiteral\("([^"]+)"\))re"),
        std::regex(
            R"re(TransportButton\(\s*TransportButton::Kind::\w+\s*,\s*QStringLiteral\("([^"]+)"\))re"),
        std::regex(
            R"re(new (?:SettingsRow|SettingCard|TabHeader)\(\s*QStringLiteral\("([^"]+)"\))re"),
    };
    std::vector<std::string> missing;
    std::size_t found = 0;
    for (const auto &file : filesUnder(kRoot / "src", {".cpp", ".hpp"})) {
        std::istringstream lines(readText(file));
        std::string line;
        while (std::getline(lines, line)) {
            for (const std::regex &pattern : patterns) {
                for (auto it = std::sregex_iterator(line.begin(), line.end(), pattern);
                     it != std::sregex_iterator(); ++it) {
                    ++found;
                    const std::string icon = (*it)[1].str();
                    if (!icon.empty() && !icons.contains(icon)) {
                        missing.push_back(icon + " (" +
                                          std::filesystem::relative(file, kRoot).generic_string() +
                                          ")");
                    }
                }
            }
        }
    }
    CHECK(found > 0);
    INFO("icons used in the code but missing from assets/icons:" << joined(missing));
    CHECK(missing.empty());
}

TEST_CASE("source includes respect the layered architecture boundaries") {
    const std::regex includeRe(R"re(^\s*#include\s+"([^"]+)")re");
    std::vector<std::string> violations;

    auto startsWith = [](const std::string &text, const std::string &prefix) {
        return text.rfind(prefix, 0) == 0;
    };

    for (const auto &file : filesUnder(kRoot / "src", {".cpp", ".hpp"})) {
        const std::string rel = std::filesystem::relative(file, kRoot / "src").generic_string();
        std::istringstream lines(readText(file));
        std::string line;
        int lineNo = 0;
        while (std::getline(lines, line)) {
            ++lineNo;
            std::smatch match;
            if (!std::regex_search(line, match, includeRe)) {
                continue;
            }
            const std::string inc = match[1].str();
            const std::string loc = "src/" + rel + ":" + std::to_string(lineNo) + " -> " + inc;

            if (startsWith(rel, "core/") || startsWith(rel, "model/") || startsWith(rel, "net/") ||
                startsWith(rel, "platform/") || startsWith(rel, "codec/")) {
                if (startsWith(inc, "app/") || startsWith(inc, "services/") ||
                    startsWith(inc, "ui/") || startsWith(inc, "crash/")) {
                    violations.push_back(loc);
                }
            } else if (startsWith(rel, "services/")) {
                if (startsWith(inc, "app/") || startsWith(inc, "ui/") ||
                    startsWith(inc, "crash/")) {
                    violations.push_back(loc);
                }
            } else if (startsWith(rel, "ui/core/") || startsWith(rel, "ui/render/") ||
                       startsWith(rel, "ui/widgets/")) {
                if (startsWith(inc, "app/") || startsWith(inc, "services/") ||
                    startsWith(inc, "crash/") || startsWith(inc, "ui/controllers/") ||
                    startsWith(inc, "ui/main_window/") || startsWith(inc, "ui/settings_window/") ||
                    startsWith(inc, "ui/editor_window/") || startsWith(inc, "ui/issue/")) {
                    violations.push_back(loc);
                }
            } else if (startsWith(rel, "ui/controllers/")) {
                if (startsWith(inc, "app/") || startsWith(inc, "crash/") ||
                    startsWith(inc, "ui/main_window/") || startsWith(inc, "ui/settings_window/") ||
                    startsWith(inc, "ui/editor_window/") || startsWith(inc, "ui/issue/")) {
                    violations.push_back(loc);
                }
            } else if (startsWith(rel, "ui/main_window/") ||
                       startsWith(rel, "ui/settings_window/") ||
                       startsWith(rel, "ui/editor_window/") || startsWith(rel, "ui/issue/")) {
                if (startsWith(inc, "app/") || startsWith(inc, "crash/")) {
                    violations.push_back(loc);
                }
                for (const char *win :
                     {"ui/main_window/", "ui/settings_window/", "ui/editor_window/", "ui/issue/"}) {
                    if (startsWith(inc, win) && !startsWith(rel, win)) {
                        violations.push_back(loc);
                    }
                }
            }
        }
    }

    INFO("architecture dependency boundary violations:" << joined(violations));
    CHECK(violations.empty());
}
