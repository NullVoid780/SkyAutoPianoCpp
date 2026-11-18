#include <catch2/catch_test_macros.hpp>

#include <miniz.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "net/zip_extract.hpp"
#include "services/update/update_service.hpp"

using namespace sky;

namespace {

std::filesystem::path freshDirectory(const char *name)
{
    const auto dir = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void writeFile(const std::filesystem::path &file, const std::string &content)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << content;
}

std::string readFile(const std::filesystem::path &file)
{
    std::ifstream input(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

}  // namespace

namespace {

constexpr const char *kTestSha256 = "fef6689acd9011dc45034ad2bc7570f06536086f220cd9aacbfba73170814cc9";

Json sampleReleases()
{
    Json releases = Json::parse(R"([
        {"tag_name": "v2.2.0", "draft": true, "body": "- Draft",
         "assets": [{"name": "SkyAutoPiano-2.2.0-setup.exe", "browser_download_url": "https://github.com/x/draft.exe"}]},
        {"tag_name": "v2.1.1", "prerelease": true, "body": "- Beta",
         "assets": [{"name": "SkyAutoPiano-2.1.1-setup.exe", "browser_download_url": "https://github.com/x/beta.exe"}]},
        {"tag_name": "v2.1.0", "html_url": "https://github.com/NullVoid780/SkyAutoPianoCpp/releases/tag/v2.1.0",
         "body": "### Fixed\r\n\r\n- Keys no longer get stuck\r\n- **Faster** start\r\n\r\n**Full Changelog**: https://github.com/x/compare/v2.0.9...v2.1.0",
         "assets": [
            {"name": "SkyAutoPiano-2.1.0.zip", "browser_download_url": "https://github.com/x/app.zip", "size": 5},
            {"name": "SkyAutoPiano-2.1.0-setup.exe", "size": 20,
             "browser_download_url": "https://github.com/NullVoid780/SkyAutoPianoCpp/releases/download/v2.1.0/SkyAutoPiano-2.1.0-setup.exe"}]},
        {"tag_name": "v2.0.9", "body": "- Older fix",
         "assets": [{"name": "SkyAutoPiano-2.0.9-setup.exe", "browser_download_url": "https://github.com/x/2.0.9.exe"}]},
        {"tag_name": "v2.0.10", "body": "- Newer fix than 2.0.9",
         "assets": [{"name": "SkyAutoPiano-2.0.10-setup.exe", "browser_download_url": "https://github.com/x/2.0.10.exe"}]}])");
    releases[2]["assets"][1]["digest"] = std::string("sha256:") + kTestSha256;
    return releases;
}

}  // namespace

TEST_CASE("version comparison is numeric, not textual")
{
    CHECK(compareVersions("1.3.13", "1.3.9") > 0);
    CHECK(compareVersions("1.3.13", "2.0.0") < 0);
    CHECK(compareVersions("2.0.0", "2.0") == 0);
    CHECK(compareVersions("v2.1.0", "2.0.9") > 0);
    CHECK(compareVersions("2.0.0-beta", "2.0.0") == 0);
    CHECK(compareVersions(" 1.10.0 ", "1.9.0") > 0);
}

TEST_CASE("the newest published release with an installer is offered")
{
    auto check = UpdateService::evaluateReleases(sampleReleases(), "2.0.0");
    REQUIRE(check.ok());
    CHECK(check.value().available);
    CHECK(check.value().latestVersion == "2.1.0");
    CHECK_FALSE(check.value().critical);
    CHECK(check.value().notes == "### Fixed\n\n- Keys no longer get stuck\n- **Faster** start");
    CHECK(check.value().releasePage == "https://github.com/NullVoid780/SkyAutoPianoCpp/releases/tag/v2.1.0");
    REQUIRE(check.value().package);
    CHECK(check.value().package->kind == PackageKind::Installer);
    CHECK(check.value().package->sha256 == kTestSha256);
    CHECK(check.value().package->size == 20);

    const auto &history = check.value().history;
    REQUIRE(history.size() == 3);
    CHECK(history[0].version == "2.1.0");
    CHECK(history[1].version == "2.0.10");
    CHECK(history[2].version == "2.0.9");
    const auto missed = missedVersions(history, "2.0.9", "2.1.0");
    REQUIRE(missed.size() == 1);
    CHECK(missed[0].markdown == "- Newer fix than 2.0.9");
}

TEST_CASE("the same or an older version is not offered, and releases without an installer are skipped")
{
    auto same = UpdateService::evaluateReleases(sampleReleases(), "2.1.0");
    REQUIRE(same.ok());
    CHECK_FALSE(same.value().available);
    CHECK_FALSE(same.value().package);

    Json noInstaller = sampleReleases();
    noInstaller[2]["assets"] = Json::array();
    auto older = UpdateService::evaluateReleases(noInstaller, "2.0.0");
    REQUIRE(older.ok());
    CHECK(older.value().latestVersion == "2.0.10");

    Json insecure = Json::parse(R"([{"tag_name": "v3.0.0",
        "assets": [{"name": "SkyAutoPiano-3.0.0-setup.exe", "browser_download_url": "http://example.com/setup.exe"},
                   {"name": "OtherTool-3.0.0-setup.exe", "browser_download_url": "https://example.com/other.exe"}]}])");
    CHECK(UpdateService::evaluateReleases(insecure, "2.0.0").error().code == "UPDATE_NOT_PUBLISHED");
    CHECK(UpdateService::evaluateReleases(Json::array(), "2.0.0").error().code == "UPDATE_NOT_PUBLISHED");
}

TEST_CASE("release notes drop GitHub's compare link and read the critical marker")
{
    bool critical = false;
    CHECK(cleanReleaseNotes("<!-- critical -->\r\n- Fixes a crash\r\n", &critical) == "- Fixes a crash");
    CHECK(critical);
    CHECK(cleanReleaseNotes("- Small fix\n<!-- just a note -->\n**Full Changelog**: link", &critical) == "- Small fix");
    CHECK_FALSE(critical);
    CHECK(cleanReleaseNotes("- Shown in the app\r\n\r\n---\r\n[Open an issue](https://example.com)") ==
          "- Shown in the app");
}

TEST_CASE("without the API the version comes from the release page address")
{
    CHECK(versionFromReleaseUrl("https://github.com/a/b/releases/tag/v2.0.1") == "2.0.1");
    CHECK(versionFromReleaseUrl("https://github.com/a/b/releases/tag/v2.0.1?x=1") == "2.0.1");
    CHECK(versionFromReleaseUrl("https://github.com/a/b/releases") == "");
    CHECK(versionFromReleaseUrl("https://github.com/a/b/releases/tag/nightly") == "");

    const std::vector<VersionNotes> changelog = {{"2.0.2", "- Two"}, {"2.0.1", "- One"}};
    const VersionCheck check = UpdateService::evaluateLatestTag("2.0.2", "2.0.0", changelog);
    CHECK(check.available);
    CHECK(check.notes == "- Two");
    REQUIRE(check.package);
    CHECK(check.package->url.ends_with("/releases/download/v2.0.2/SkyAutoPiano-2.0.2-setup.exe"));
    CHECK(check.package->sha256.empty());
    CHECK_FALSE(UpdateService::evaluateLatestTag("2.0.0", "2.0.0", {}).available);
}

TEST_CASE("the changelog gives notes per version and the versions the user skipped")
{
    const std::string changelog = "# Changelog\n\n## v2.3.0\n\n### Added\n\n- Playlists\n\n"
                                  "## v2.2.0\n\n- Fixed **speed**\n\n## v2.1.0\n\n- First fixes\n\n## v2.0.0\n\n- First release\n";
    const auto versions = parseChangelog(changelog);
    REQUIRE(versions.size() == 4);
    CHECK(versions[0].version == "2.3.0");
    CHECK(versions[0].markdown == "### Added\n\n- Playlists");

    const auto missed = missedVersions(versions, "2.0.0", "2.3.0");
    REQUIRE(missed.size() == 2);
    CHECK(missed[0].version == "2.2.0");
    CHECK(missed[1].version == "2.1.0");
    REQUIRE(findVersionNotes(versions, "v2.2.0"));
    CHECK(findVersionNotes(versions, "9.9.9") == nullptr);

    const std::string notes = composeUpdateNotes(versions[0].markdown, missed);
    CHECK(notes.rfind("## What's new?", 0) == 0);
    CHECK(notes.find("## Other updates you've missed") != std::string::npos);
    CHECK(notes.find("### Version 2.2.0\n\n- Fixed **speed**") != std::string::npos);
    CHECK(composeUpdateNotes("", {}).find("No release notes") != std::string::npos);
}

TEST_CASE("staged files replace the app but never data, config or logs")
{
    const auto root = freshDirectory("sky_update_apply_test");
    const auto app = root / "app";
    const auto staging = root / "staging";
    writeFile(app / "SkyAutoPiano.exe", "old");
    writeFile(app / "data" / "listSheet.json", "user data");
    writeFile(staging / "SkyAutoPiano.exe", "new");
    writeFile(staging / "platforms" / "qwindows.dll", "plugin");
    writeFile(staging / "data" / "listSheet.json", "factory data");
    writeFile(staging / "config" / "config.json", "factory config");

    auto applied = UpdateService::applyStagedFiles(staging, app, root / "cleanup-files.json");
    REQUIRE(applied.ok());
    CHECK(applied.value().size() == 2);
    CHECK(readFile(app / "SkyAutoPiano.exe") == "new");
    CHECK(readFile(app / "platforms" / "qwindows.dll") == "plugin");
    CHECK(readFile(app / "data" / "listSheet.json") == "user data");
    CHECK_FALSE(std::filesystem::exists(app / "config" / "config.json"));
    CHECK_FALSE(std::filesystem::exists(staging));
    std::filesystem::remove_all(root);
}

TEST_CASE("zip extraction writes files and rejects paths that escape the destination")
{
    const auto root = freshDirectory("sky_zip_test");
    const auto good = root / "good.zip";
    const std::string content = "hello";
    REQUIRE(mz_zip_add_mem_to_archive_file_in_place(good.string().c_str(), "folder/file.txt", content.data(),
                                                    content.size(), nullptr, 0, static_cast<mz_uint>(MZ_DEFAULT_LEVEL)));
    auto extracted = extractZip(good, root / "out");
    REQUIRE(extracted.ok());
    REQUIRE(extracted.value().size() == 1);
    CHECK(readFile(root / "out" / "folder" / "file.txt") == "hello");

    mz_zip_archive writer{};
    REQUIRE(mz_zip_writer_init_heap(&writer, 0, 0));
    REQUIRE(mz_zip_writer_add_mem(&writer, "../escaped.txt", content.data(), content.size(), static_cast<mz_uint>(MZ_DEFAULT_LEVEL)));
    void *buffer = nullptr;
    std::size_t size = 0;
    REQUIRE(mz_zip_writer_finalize_heap_archive(&writer, &buffer, &size));
    {
        std::ofstream(root / "evil.zip", std::ios::binary).write(static_cast<const char *>(buffer),
                                                                 static_cast<std::streamsize>(size));
    }
    mz_zip_writer_end(&writer);
    auto rejected = extractZip(root / "evil.zip", root / "out2");
    CHECK_FALSE(rejected.ok());
    CHECK_FALSE(std::filesystem::exists(root / "escaped.txt"));
    std::filesystem::remove_all(root);
}
