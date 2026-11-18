#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "crash/crash_report.hpp"

namespace sky {

std::vector<CrashModule> collectModules(HANDLE process);
CrashSystem collectSystem();
CrashProcess collectProcess(HANDLE process, const CrashContext &context);
std::string threadDescription(DWORD threadId);
std::string sha256File(const std::filesystem::path &file);
const CrashModule *moduleContaining(const std::vector<CrashModule> &modules, std::uint64_t address);

}  // namespace sky
