#include "crash/crash_archive.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <miniz.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

namespace sky {

namespace {

constexpr std::size_t kLogTailBytes = 1024 * 1024;
constexpr int kReportSchema = 1;

std::optional<std::string> readAll(const std::filesystem::path &file)
{
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), {});
}

std::string readTail(const std::filesystem::path &file, std::size_t maxBytes)
{
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    if (!input) {
        return {};
    }
    const std::streamoff size = input.tellg();
    const std::streamoff start = std::max<std::streamoff>(0, size - static_cast<std::streamoff>(maxBytes));
    input.seekg(start);
    std::string text(static_cast<std::size_t>(size - start), '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (start > 0) {
        if (const auto newline = text.find('\n'); newline != std::string::npos) {
            text.erase(0, newline + 1);
        }
    }
    return text;
}

std::string utcNow()
{
    SYSTEMTIME now{};
    GetSystemTime(&now);
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", now.wYear, now.wMonth, now.wDay,
                  now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    return buffer;
}

nlohmann::ordered_json readJsonFile(const std::filesystem::path &file)
{
    const auto text = readAll(file);
    if (!text) {
        return nlohmann::ordered_json::object();
    }
    auto parsed = nlohmann::ordered_json::parse(*text, nullptr, false);
    return parsed.is_object() ? parsed : nlohmann::ordered_json::object();
}

nlohmann::ordered_json environmentFor(const nlohmann::ordered_json &report, const std::filesystem::path &home)
{
    using Json = nlohmann::ordered_json;
    const Json app = report.value("app", Json::object());
    const Json system = report.value("system", Json::object());
    const Json state = report.value("state", Json::object());
    const Json uiState = readJsonFile(home / "data" / "ui-state.json");
    Json environment = Json::object();
    environment["app"] = Json{{"name", "SkyAutoPiano"}, {"version", app.value("version", std::string())}};
    environment["system"] = Json{{"os", system.value("os", std::string())},
                                 {"arch", system.value("arch", std::string())},
                                 {"locale", system.value("locale", std::string())}};
    environment["hardware"] = Json{{"cpu", system.value("cpu", std::string())},
                                   {"threads", system.value("cores", 0)},
                                   {"ramMb", system.value("ram_mb", 0)}};
    environment["theme"] = Json{{"activeId", uiState.value("themeId", std::string("builtin-default"))},
                                {"kind", uiState.value("theme", state.value("theme", std::string("dark")))},
                                {"followWindows", uiState.value("followSystemTheme", true)}};
    environment["playback"] = Json{{"mode", state.value("playback_mode", std::string("lite"))},
                                   {"speed", state.value("speed", 1.0)},
                                   {"longPress", state.value("long_press", false)}};
    return environment;
}

}  // namespace

std::filesystem::path crashAppHome(const std::filesystem::path &executable)
{
    wchar_t *home = nullptr;
    std::size_t length = 0;
    if (_wdupenv_s(&home, &length, L"SKYAUTOPIANO_HOME") == 0 && home && *home) {
        std::filesystem::path chosen(home);
        std::free(home);
        return chosen;
    }
    std::free(home);
    return executable.parent_path();
}

bool writeCrashArchive(const std::filesystem::path &target, const CrashArchiveInput &input)
{
    using Json = nlohmann::ordered_json;
    Json manifest = Json::object();
    manifest["schema"] = kReportSchema;
    manifest["kind"] = "crash";
    manifest["createdAt"] = utcNow();
    manifest["crashId"] = input.id;
    manifest["crash"] = input.report;
    const std::string dumpName = "crash/" + input.id + ".dmp";
    std::optional<std::string> dumpBytes;
    if (input.dump) {
        dumpBytes = readAll(*input.dump);
        if (dumpBytes) {
            manifest["dump"] = dumpName;
        }
    }
    manifest["environment"] = environmentFor(input.report, input.appHome);

    std::vector<std::pair<std::string, std::string>> entries;
    entries.emplace_back("report.json", manifest.dump(2));
    entries.emplace_back("crash/" + input.id + ".json", input.report.dump(2));
    if (dumpBytes) {
        entries.emplace_back(dumpName, std::move(*dumpBytes));
    }
    const std::pair<const char *, std::filesystem::path> environmentFiles[] = {
        {"env/config.json", input.appHome / "config" / "config.json"},
        {"env/ui-state.json", input.appHome / "data" / "ui-state.json"},
        {"env/themes.json", input.appHome / "data" / "themes.json"},
        {"env/playlists.json", input.appHome / "data" / "playlists.json"},
    };
    for (const auto &[name, file] : environmentFiles) {
        if (auto bytes = readAll(file)) {
            entries.emplace_back(name, std::move(*bytes));
        }
    }
    entries.emplace_back("log.txt", readTail(input.appHome / "logs" / "skyautopiano.log", kLogTailBytes));

    std::FILE *handle = nullptr;
    if (_wfopen_s(&handle, target.c_str(), L"wb") != 0 || !handle) {
        return false;
    }
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_cfile(&zip, handle, 0)) {
        std::fclose(handle);
        return false;
    }
    bool ok = true;
    for (const auto &[name, bytes] : entries) {
        if (!mz_zip_writer_add_mem(&zip, name.c_str(), bytes.data(), bytes.size(),
                                    static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION))) {
            ok = false;
            break;
        }
    }
    ok = ok && mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    std::fclose(handle);
    if (!ok) {
        std::error_code error;
        std::filesystem::remove(target, error);
    }
    return ok;
}

}  // namespace sky
