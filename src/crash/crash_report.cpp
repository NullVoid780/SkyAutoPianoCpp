#include "crash/crash_report.hpp"

#include <array>
#include <cstdio>
#include <utility>

namespace sky {

namespace {

constexpr std::array<std::pair<DWORD, const char *>, 22> kExceptionNames = {{
    {EXCEPTION_ACCESS_VIOLATION, "EXCEPTION_ACCESS_VIOLATION"},
    {EXCEPTION_ARRAY_BOUNDS_EXCEEDED, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"},
    {EXCEPTION_BREAKPOINT, "EXCEPTION_BREAKPOINT"},
    {EXCEPTION_DATATYPE_MISALIGNMENT, "EXCEPTION_DATATYPE_MISALIGNMENT"},
    {EXCEPTION_FLT_DIVIDE_BY_ZERO, "EXCEPTION_FLT_DIVIDE_BY_ZERO"},
    {EXCEPTION_FLT_INVALID_OPERATION, "EXCEPTION_FLT_INVALID_OPERATION"},
    {EXCEPTION_FLT_OVERFLOW, "EXCEPTION_FLT_OVERFLOW"},
    {EXCEPTION_ILLEGAL_INSTRUCTION, "EXCEPTION_ILLEGAL_INSTRUCTION"},
    {EXCEPTION_IN_PAGE_ERROR, "EXCEPTION_IN_PAGE_ERROR"},
    {EXCEPTION_INT_DIVIDE_BY_ZERO, "EXCEPTION_INT_DIVIDE_BY_ZERO"},
    {EXCEPTION_INT_OVERFLOW, "EXCEPTION_INT_OVERFLOW"},
    {EXCEPTION_INVALID_DISPOSITION, "EXCEPTION_INVALID_DISPOSITION"},
    {EXCEPTION_NONCONTINUABLE_EXCEPTION, "EXCEPTION_NONCONTINUABLE_EXCEPTION"},
    {EXCEPTION_PRIV_INSTRUCTION, "EXCEPTION_PRIV_INSTRUCTION"},
    {EXCEPTION_STACK_OVERFLOW, "EXCEPTION_STACK_OVERFLOW"},
    {0xC0000374, "STATUS_HEAP_CORRUPTION"},
    {0xC0000409, "STATUS_STACK_BUFFER_OVERRUN"},
    {kTerminateCode, "CPP_TERMINATE"},
    {kAbortCode, "ABORT"},
    {kInvalidParameterCode, "CRT_INVALID_PARAMETER"},
    {kPureCallCode, "PURE_VIRTUAL_CALL"},
    {kOutOfMemoryCode, "OUT_OF_MEMORY"},
}};

}  // namespace

std::string hex(std::uint64_t value, int width)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%0*llX", width, static_cast<unsigned long long>(value));
    return buffer;
}

std::string crashReasonName(CrashReason reason, bool unexpectedExit)
{
    if (unexpectedExit) {
        return "unexpected_exit";
    }
    switch (reason) {
    case CrashReason::SehException:
        return "seh_exception";
    case CrashReason::CppTerminate:
        return "cpp_terminate";
    case CrashReason::Abort:
        return "abort";
    case CrashReason::InvalidParameter:
        return "invalid_parameter";
    case CrashReason::PureCall:
        return "pure_call";
    case CrashReason::OutOfMemory:
        return "out_of_memory";
    case CrashReason::None:
        break;
    }
    return "unknown";
}

std::string exceptionCodeName(DWORD code)
{
    for (const auto &[value, name] : kExceptionNames) {
        if (value == code) {
            return name;
        }
    }
    return "UNKNOWN_EXCEPTION";
}

std::string describeException(const EXCEPTION_RECORD &record)
{
    std::string text = exceptionCodeName(record.ExceptionCode);
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2) {
        const ULONG_PTR kind = record.ExceptionInformation[0];
        const char *verb = kind == 0 ? "reading" : kind == 1 ? "writing" : "executing";
        text += std::string(" ") + verb + " " + hex(record.ExceptionInformation[1], 16);
    }
    return text;
}

nlohmann::ordered_json buildCrashReport(const CrashReportInput &input)
{
    using Json = nlohmann::ordered_json;
    Json report = Json::object();
    report["schema"] = 1;
    report["app"] = Json{{"name", "SkyAutoPiano"}, {"version", input.appVersion}, {"build", input.build},
                         {"channel", "release"}};

    Json crash = Json::object();
    crash["reason"] = crashReasonName(input.reason, input.unexpectedExit);
    if (!input.unexpectedExit) {
        crash["code"] = hex(input.code, 8);
        crash["description"] = input.description;
        crash["address"] = hex(input.address, 16);
        crash["module"] = input.module;
        crash["module_offset"] = hex(input.moduleOffset);
        crash["thread_id"] = input.threadId;
        crash["thread_name"] = input.threadName;
    }
    report["crash"] = std::move(crash);
    report["stack"] = input.stack;

    Json modules = Json::array();
    for (const CrashModule &module : input.modules) {
        modules.push_back(Json{{"name", module.name}, {"base", hex(module.base)}, {"size", module.size},
                               {"version", module.version}});
    }
    report["modules"] = std::move(modules);
    report["system"] = Json{{"os", input.system.os},       {"arch", input.system.arch},
                            {"cpu", input.system.cpu},     {"cores", input.system.cores},
                            {"ram_mb", input.system.ramMb}, {"locale", input.system.locale}};
    report["process"] = Json{{"uptime_ms", input.process.uptimeMs},
                             {"peak_working_set_mb", input.process.peakWorkingSetMb},
                             {"handles", input.process.handles},
                             {"threads", input.process.threads}};
    if (input.state) {
        const CrashState &s = *input.state;
        report["state"] = Json{{"playback_mode", s.playbackMode == 1 ? "strict" : "lite"},
                               {"is_playing", s.playing != 0},
                               {"speed", s.speed},
                               {"long_press", s.longPress != 0},
                               {"note_count", s.noteCount},
                               {"theme", s.theme == 1 ? "dark" : "light"},
                               {"custom_keyboard", s.customKeyboard != 0},
                               {"library_size", s.librarySize}};
    }
    Json crumbs = Json::array();
    for (const BreadcrumbEntry &entry : input.breadcrumbs) {
        crumbs.push_back(Json{{"t", entry.timeMs}, {"c", entry.category}, {"m", entry.message}});
    }
    report["breadcrumbs"] = std::move(crumbs);
    if (input.dump) {
        report["dump"] = Json{{"file", input.dump->file}, {"bytes", input.dump->bytes}, {"sha256", input.dump->sha256}};
    } else {
        report["dump"] = nullptr;
    }
    return report;
}

}  // namespace sky
