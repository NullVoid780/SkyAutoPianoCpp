#pragma once

#include <filesystem>
#include <string>

namespace sky {

enum class CrashDialogChoice { Close, Restart };

struct CrashDialogContent {
    std::wstring reason;
    std::wstring reportName;
    std::filesystem::path folder;
    std::wstring details;
};

CrashDialogChoice showCrashDialog(const CrashDialogContent &content);

}  // namespace sky
