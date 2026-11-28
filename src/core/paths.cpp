#include "core/paths.hpp"

#include <windows.h>

#include <array>
#include <cstdlib>

#include "core/file_system.hpp"

namespace sky {

namespace {

std::filesystem::path executableDirectory() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

}  // namespace

std::filesystem::path Paths::appDirectory() {
    static const std::filesystem::path directory = [] {
        wchar_t *overridePath = nullptr;
        std::size_t length = 0;
        if (_wdupenv_s(&overridePath, &length, L"SKYAUTOPIANO_HOME") == 0 && overridePath &&
            *overridePath) {
            std::filesystem::path chosen(overridePath);
            std::free(overridePath);
            return chosen;
        }
        std::free(overridePath);
        return executableDirectory();
    }();
    return directory;
}

std::filesystem::path Paths::configDirectory() {
    return appDirectory() / "config";
}

std::filesystem::path Paths::dataDirectory() {
    return appDirectory() / "data";
}

std::filesystem::path Paths::logDirectory() {
    return appDirectory() / "logs";
}

std::filesystem::path Paths::logFile() {
    return logDirectory() / "skyautopiano.log";
}

Result<void> Paths::ensureDirectories() {
    for (const std::filesystem::path &directory :
         {configDirectory(), dataDirectory(), logDirectory()}) {
        if (auto created = ensureDir(directory); !created) {
            return created;
        }
    }
    return {};
}

}  // namespace sky
