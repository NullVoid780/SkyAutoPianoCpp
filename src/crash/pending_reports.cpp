#include "crash/pending_reports.hpp"

#include "crash/crash_context.hpp"

#include <objbase.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <system_error>
#include <vector>

namespace sky {

std::filesystem::path crashDirectory()
{
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

void pruneCrashReports(const std::filesystem::path &directory, std::size_t keep, std::uintmax_t maxDumpBytes)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        return;
    }
    std::map<std::string, std::vector<std::filesystem::path>> reports;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_regular_file(error)) {
            continue;
        }
        const std::filesystem::path path = entry.path();
        const std::wstring extension = path.extension().wstring();
        if (extension != L".json" && extension != L".dmp") {
            continue;
        }
        if (extension == L".dmp" && entry.file_size(error) > maxDumpBytes) {
            std::filesystem::remove(path, error);
            continue;
        }
        reports[path.stem().string()].push_back(path);
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

}  // namespace sky
