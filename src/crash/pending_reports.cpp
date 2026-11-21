#include "crash/pending_reports.hpp"

#include "crash/crash_context.hpp"

#include <objbase.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <system_error>
#include <vector>

namespace sky {

std::filesystem::path crashDirectory()
{
    wchar_t *home = nullptr;
    std::size_t length = 0;
    if (_wdupenv_s(&home, &length, L"SKYAUTOPIANO_HOME") == 0 && home && *home) {
        std::filesystem::path sandboxed = std::filesystem::path(home) / L"crashes";
        std::free(home);
        return sandboxed;
    }
    std::free(home);
    PWSTR base = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base))) {
        result = std::filesystem::path(base) / L"SkyAutoPiano" / L"crashes";
    }
    CoTaskMemFree(base);
    return result;
}

std::string newReportId()
{
    SYSTEMTIME now{};
    GetLocalTime(&now);
    GUID guid{};
    CoCreateGuid(&guid);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04u%02u%02u-%02u%02u%02u-%08lx", now.wYear, now.wMonth, now.wDay,
                  now.wHour, now.wMinute, now.wSecond, guid.Data1);
    return buffer;
}

namespace {

bool isCrashFile(const std::filesystem::path &path)
{
    const std::wstring extension = path.extension().wstring();
    return extension == L".json" || extension == L".dmp" || extension == L".sapcr";
}

}  // namespace

void pruneCrashReports(const std::filesystem::path &directory, std::size_t keep, std::uintmax_t maxDumpBytes,
                       std::chrono::hours maxAge)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        return;
    }
    const auto cutoff = std::filesystem::file_time_type::clock::now() - maxAge;
    std::map<std::string, std::vector<std::filesystem::path>> reports;
    std::set<std::string> expired;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_regular_file(error)) {
            continue;
        }
        const std::filesystem::path path = entry.path();
        if (!isCrashFile(path)) {
            continue;
        }
        if (path.extension() == L".dmp" && entry.file_size(error) > maxDumpBytes) {
            std::filesystem::remove(path, error);
            continue;
        }
        std::error_code timeError;
        if (const auto written = entry.last_write_time(timeError); !timeError && written < cutoff) {
            expired.insert(path.stem().string());
        }
        reports[path.stem().string()].push_back(path);
    }
    for (const std::string &id : expired) {
        for (const auto &file : reports[id]) {
            std::filesystem::remove(file, error);
        }
        reports.erase(id);
    }
    if (reports.size() <= keep) {
        return;
    }
    std::size_t excess = reports.size() - keep;
    for (const auto &[id, files] : reports) {
        if (excess == 0) {
            break;
        }
        for (const auto &file : files) {
            std::filesystem::remove(file, error);
        }
        --excess;
    }
}

std::size_t deleteCrashReports(const std::filesystem::path &directory)
{
    std::error_code error;
    std::set<std::string> removed;
    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
        if (entry.is_regular_file(error) && isCrashFile(entry.path())) {
            files.push_back(entry.path());
        }
    }
    for (const auto &file : files) {
        if (std::filesystem::remove(file, error)) {
            removed.insert(file.stem().string());
        }
    }
    return removed.size();
}

}  // namespace sky
