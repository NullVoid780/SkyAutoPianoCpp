#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "crash/breadcrumbs.hpp"
#include "crash/crash_context.hpp"

namespace sky {

struct CrashModule {
    std::string name;
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::string version;
};

struct CrashSystem {
    std::string os;
    std::string arch = "x64";
    std::string cpu;
    int cores = 0;
    std::uint64_t ramMb = 0;
    std::string locale;
};

struct CrashProcess {
    std::uint64_t uptimeMs = 0;
    std::uint64_t peakWorkingSetMb = 0;
    std::uint32_t handles = 0;
    std::uint32_t threads = 0;
};

struct CrashDump {
    std::string file;
    std::uint64_t bytes = 0;
    std::string sha256;
};

struct CrashReportInput {
    std::string appVersion;
    std::string build;
    CrashReason reason = CrashReason::None;
    bool unexpectedExit = false;
    DWORD code = 0;
    std::string description;
    std::uint64_t address = 0;
    std::string module;
    std::uint64_t moduleOffset = 0;
    DWORD threadId = 0;
    std::string threadName;
    std::vector<std::string> stack;
    std::vector<CrashModule> modules;
    CrashSystem system;
    CrashProcess process;
    std::optional<CrashState> state;
    std::vector<BreadcrumbEntry> breadcrumbs;
    std::optional<CrashDump> dump;
};

std::string crashReasonName(CrashReason reason, bool unexpectedExit);
std::string exceptionCodeName(DWORD code);
std::string describeException(const EXCEPTION_RECORD &record);
std::string hex(std::uint64_t value, int width = 0);
nlohmann::ordered_json buildCrashReport(const CrashReportInput &input);

}  // namespace sky
