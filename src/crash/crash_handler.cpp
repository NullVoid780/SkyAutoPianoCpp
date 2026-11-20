#include "crash/crash_handler.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <new>
#include <string>

#include "crash/breadcrumbs.hpp"
#include "crash/crash_channel.hpp"
#include "crash/minidump_writer.hpp"
#include "crash/pending_reports.hpp"

namespace sky {

namespace {

constexpr DWORD kDumpWaitMs = 30000;
constexpr ULONG kStackGuaranteeBytes = 64 * 1024;

CrashChannel g_channel;
CrashContext *g_context = nullptr;
CrashHandler::PanicRelease g_panicRelease = nullptr;
HANDLE g_reporter = nullptr;
volatile LONG g_crashing = 0;
EXCEPTION_POINTERS g_pointers{};

void copyBounded(char *target, std::size_t capacity, const char *source) noexcept
{
    std::size_t i = 0;
    for (; source && i + 1 < capacity && source[i] != '\0'; ++i) {
        target[i] = source[i];
    }
    target[i] = '\0';
}

bool reporterAlive() noexcept
{
    return g_reporter && WaitForSingleObject(g_reporter, 0) == WAIT_TIMEOUT;
}

void writeOwnDump() noexcept
{
    const std::filesystem::path directory = crashDirectory();
    if (directory.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const std::filesystem::path file = directory / (newReportId() + ".dmp");
    writeMinidump(GetCurrentProcess(), GetCurrentProcessId(), GetCurrentThreadId(), &g_pointers, false, file);
}

[[noreturn]] void handleCrash(CrashReason reason, EXCEPTION_POINTERS *pointers) noexcept
{
    if (InterlockedCompareExchange(&g_crashing, 1, 0) != 0) {
        Sleep(INFINITE);
    }
    if (g_panicRelease) {
        g_panicRelease();
    }
    DWORD code = 0;
    if (g_context) {
        CrashContext &context = *g_context;
        context.threadId = GetCurrentThreadId();
        context.reason = static_cast<std::uint32_t>(reason);
        context.crashTick = GetTickCount64();
        if (pointers && pointers->ExceptionRecord) {
            context.record = *pointers->ExceptionRecord;
            context.record.ExceptionRecord = nullptr;
        }
        if (pointers && pointers->ContextRecord) {
            context.context = *pointers->ContextRecord;
        }
        context.exceptionCode = context.record.ExceptionCode;
        context.exceptionAddress = reinterpret_cast<std::uint64_t>(context.record.ExceptionAddress);
        g_pointers.ExceptionRecord = &context.record;
        g_pointers.ContextRecord = &context.context;
        context.exceptionPointers = reinterpret_cast<std::uint64_t>(&g_pointers);
        InterlockedExchange(&context.crashed, 1);
        code = context.exceptionCode;
    } else if (pointers && pointers->ExceptionRecord) {
        code = pointers->ExceptionRecord->ExceptionCode;
    }

    if (g_context && reporterAlive()) {
        SetEvent(g_channel.crashSignal());
        const HANDLE waits[] = {g_channel.dumpComplete(), g_reporter};
        WaitForMultipleObjects(2, waits, FALSE, kDumpWaitMs);
    } else if (g_context) {
        writeOwnDump();
    }
    TerminateProcess(GetCurrentProcess(), code ? code : 1);
    for (;;) {
        Sleep(INFINITE);
    }
}

[[noreturn]] void raiseSynthetic(CrashReason reason, DWORD code) noexcept
{
    static EXCEPTION_RECORD record;
    static CONTEXT context;
    ZeroMemory(&record, sizeof(record));
    RtlCaptureContext(&context);
    record.ExceptionCode = code;
    record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
#if defined(_M_X64)
    record.ExceptionAddress = reinterpret_cast<PVOID>(context.Rip);
#endif
    EXCEPTION_POINTERS pointers{&record, &context};
    handleCrash(reason, &pointers);
}

LONG WINAPI unhandledFilter(EXCEPTION_POINTERS *pointers)
{
    handleCrash(CrashReason::SehException, pointers);
}

LONG WINAPI vectoredFilter(EXCEPTION_POINTERS *pointers)
{
    const DWORD code = pointers->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_STACK_OVERFLOW || code == 0xC0000374 || code == 0xC0000409) {
        handleCrash(CrashReason::SehException, pointers);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void onTerminate()
{
    raiseSynthetic(CrashReason::CppTerminate, kTerminateCode);
}

void onAbort(int)
{
    raiseSynthetic(CrashReason::Abort, kAbortCode);
}

void onInvalidParameter(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t)
{
    raiseSynthetic(CrashReason::InvalidParameter, kInvalidParameterCode);
}

void onPureCall()
{
    raiseSynthetic(CrashReason::PureCall, kPureCallCode);
}

void onOutOfMemory()
{
    raiseSynthetic(CrashReason::OutOfMemory, kOutOfMemoryCode);
}

void launchReporter() noexcept
{
    wchar_t self[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, self, MAX_PATH) == 0) {
        return;
    }
    const std::filesystem::path reporter = std::filesystem::path(self).parent_path() / L"CrashReport.exe";
    std::error_code error;
    if (!std::filesystem::exists(reporter, error)) {
        return;
    }
    std::wstring command = L"\"" + reporter.wstring() + L"\" --watch " + std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr,
                       nullptr, &startup, &process)) {
        CloseHandle(process.hThread);
        g_reporter = process.hProcess;
    }
}

}  // namespace

bool CrashHandler::install(const Options &options)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    g_panicRelease = options.panicRelease;
    if (g_channel.create(GetCurrentProcessId())) {
        g_context = g_channel.context();
        copyBounded(g_context->appVersion, sizeof(g_context->appVersion), options.appVersion);
    }
    ULONG guarantee = kStackGuaranteeBytes;
    SetThreadStackGuarantee(&guarantee);
    SetUnhandledExceptionFilter(unhandledFilter);
    AddVectoredExceptionHandler(0, vectoredFilter);
    std::set_terminate(onTerminate);
    std::set_new_handler(onOutOfMemory);
    std::signal(SIGABRT, onAbort);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_invalid_parameter_handler(onInvalidParameter);
    _set_purecall_handler(onPureCall);
    return g_context != nullptr;
}

void CrashHandler::startReporter() noexcept
{
    if (g_context && !g_reporter) {
        launchReporter();
    }
}

void CrashHandler::breadcrumb(const char *category, const char *message) noexcept
{
    if (g_context) {
        writeBreadcrumb(*g_context, category, message);
    }
}

CrashState *CrashHandler::state() noexcept
{
    return g_context ? &g_context->state : nullptr;
}

std::vector<BreadcrumbEntry> CrashHandler::recentBreadcrumbs()
{
    return g_context ? readBreadcrumbs(*g_context) : std::vector<BreadcrumbEntry>{};
}


void CrashHandler::markCleanExit() noexcept
{
    if (g_context) {
        InterlockedExchange(&g_context->cleanExit, 1);
    }
}

}  // namespace sky
