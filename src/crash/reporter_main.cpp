#include "crash/crash_context.hpp"

#include <shellapi.h>

#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>

#include "crash/breadcrumbs.hpp"
#include "crash/crash_archive.hpp"
#include "crash/crash_channel.hpp"
#include "crash/crash_dialog.hpp"
#include "crash/crash_report.hpp"
#include "crash/minidump_writer.hpp"
#include "crash/pending_reports.hpp"
#include "crash/report_collector.hpp"
#include "crash/stack_walker.hpp"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' " \
                        "version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr int kOpenAttempts = 50;

std::wstring widen(const std::string &text)
{
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::wstring executablePath(HANDLE process)
{
    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    return QueryFullProcessImageNameW(process, 0, path, &size) ? std::wstring(path, size) : std::wstring();
}

void restart(const std::wstring &executable, const std::string &reportId)
{
    if (executable.empty()) {
        return;
    }
    std::wstring command = L"\"" + executable + L"\" --after-crash " + widen(reportId);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    const std::wstring directory = std::filesystem::path(executable).parent_path().wstring();
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, directory.c_str(), &startup,
                       &info)) {
        CloseHandle(info.hThread);
        CloseHandle(info.hProcess);
    }
}

sky::CrashReportInput baseInput(const sky::CrashContext &context)
{
    sky::CrashReportInput input;
    input.appVersion = std::string(context.appVersion, strnlen(context.appVersion, sizeof(context.appVersion)));
    input.state = context.state;
    input.breadcrumbs = sky::readBreadcrumbs(context);
    input.system = sky::collectSystem();
    return input;
}

bool writeReport(const std::filesystem::path &file, const nlohmann::ordered_json &report)
{
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output << report.dump(2);
    return static_cast<bool>(output);
}

int handleCrash(HANDLE process, DWORD processId, sky::CrashChannel &channel, const std::wstring &executable)
{
    const sky::CrashContext &context = *channel.context();
    const std::filesystem::path directory = sky::crashDirectory();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const std::string id = sky::newReportId();
    const std::filesystem::path dumpFile = directory / (id + ".dmp");
    const std::filesystem::path reportFile = directory / (id + ".json");

    const bool dumped = sky::writeMinidump(process, processId, context.threadId,
                                           reinterpret_cast<EXCEPTION_POINTERS *>(context.exceptionPointers), true,
                                           dumpFile);
    sky::CrashReportInput input = baseInput(context);
    input.reason = static_cast<sky::CrashReason>(context.reason);
    input.code = context.exceptionCode;
    input.description = sky::describeException(context.record);
    input.address = context.exceptionAddress;
    input.threadId = context.threadId;
    input.threadName = sky::threadDescription(context.threadId);
    input.modules = sky::collectModules(process);
    if (const sky::CrashModule *module = sky::moduleContaining(input.modules, input.address)) {
        input.module = module->name;
        input.moduleOffset = input.address - module->base;
    }
    input.stack = sky::walkStack(process, context.threadId, context.context);
    input.process = sky::collectProcess(process, context);
    if (dumped) {
        input.dump = sky::CrashDump{dumpFile.filename().string(), std::filesystem::file_size(dumpFile, error),
                                    sky::sha256File(dumpFile)};
    }
    const nlohmann::ordered_json report = sky::buildCrashReport(input);
    writeReport(reportFile, report);
    SetEvent(channel.dumpComplete());

    const std::filesystem::path archiveFile = directory / (id + sky::kCrashArchiveExtension);
    sky::CrashArchiveInput archive{id, report, std::nullopt, sky::crashAppHome(executable)};
    if (dumped) {
        archive.dump = dumpFile;
    }
    std::filesystem::path shownFile = reportFile;
    if (sky::writeCrashArchive(archiveFile, archive)) {
        std::filesystem::remove(reportFile, error);
        std::filesystem::remove(dumpFile, error);
        shownFile = archiveFile;
    }

    sky::pruneCrashReports(directory);
    if (GetEnvironmentVariableW(L"SKY_CRASH_NO_DIALOG", nullptr, 0) > 0) {
        return 0;
    }
    const std::wstring reason =
        widen(sky::exceptionCodeName(input.code)) + L" (" + widen(sky::hex(input.code, 8)) + L")";
    const sky::CrashDialogChoice choice =
        sky::showCrashDialog({reason, shownFile.filename().wstring(), directory, widen(report.dump(2))});
    if (choice == sky::CrashDialogChoice::Restart) {
        WaitForSingleObject(process, 5000);
        restart(executable, id);
    }
    return 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    DWORD processId = 0;
    for (int i = 1; argv && i + 1 < argc; ++i) {
        if (std::wcscmp(argv[i], L"--watch") == 0) {
            processId = static_cast<DWORD>(std::wcstoul(argv[i + 1], nullptr, 10));
        }
    }
    LocalFree(argv);
    if (processId == 0) {
        return 2;
    }
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE,
                                 FALSE, processId);
    if (!process) {
        return 3;
    }
    sky::CrashChannel channel;
    bool opened = false;
    for (int attempt = 0; attempt < kOpenAttempts && !opened; ++attempt) {
        opened = channel.open(processId);
        if (!opened) {
            Sleep(100);
        }
    }
    if (!opened) {
        CloseHandle(process);
        return 4;
    }
    const std::wstring executable = executablePath(process);
    const HANDLE waits[] = {channel.crashSignal(), process};
    const DWORD signaled = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    int result = 0;
    if (signaled == WAIT_OBJECT_0) {
        result = handleCrash(process, processId, channel, executable);
    }
    CloseHandle(process);
    return result;
}
