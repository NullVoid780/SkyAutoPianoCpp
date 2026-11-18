#include "crash/report_collector.hpp"

#include <bcrypt.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

namespace sky {

namespace {

std::string narrow(const std::wstring &text)
{
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0,
                                           nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), length, nullptr,
                        nullptr);
    return result;
}

std::string fileVersion(const std::wstring &path)
{
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0) {
        return {};
    }
    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) {
        return {};
    }
    VS_FIXEDFILEINFO *info = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&info), &length) || !info) {
        return {};
    }
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                  HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
    return buffer;
}

std::string registryString(HKEY root, const wchar_t *key, const wchar_t *value)
{
    wchar_t buffer[256]{};
    DWORD bytes = sizeof(buffer);
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buffer, &bytes) != ERROR_SUCCESS) {
        return {};
    }
    return narrow(buffer);
}

}  // namespace

std::vector<CrashModule> collectModules(HANDLE process)
{
    std::vector<CrashModule> modules;
    HMODULE handles[512];
    DWORD needed = 0;
    if (!EnumProcessModulesEx(process, handles, sizeof(handles), &needed, LIST_MODULES_ALL)) {
        return modules;
    }
    const DWORD count = std::min<DWORD>(needed / sizeof(HMODULE), 512);
    for (DWORD i = 0; i < count; ++i) {
        wchar_t path[MAX_PATH]{};
        MODULEINFO info{};
        if (!GetModuleFileNameExW(process, handles[i], path, MAX_PATH) ||
            !GetModuleInformation(process, handles[i], &info, sizeof(info))) {
            continue;
        }
        const std::filesystem::path file(path);
        modules.push_back(CrashModule{narrow(file.filename().wstring()),
                                      reinterpret_cast<std::uint64_t>(info.lpBaseOfDll), info.SizeOfImage,
                                      fileVersion(path)});
    }
    return modules;
}

const CrashModule *moduleContaining(const std::vector<CrashModule> &modules, std::uint64_t address)
{
    for (const CrashModule &module : modules) {
        if (address >= module.base && address < module.base + module.size) {
            return &module;
        }
    }
    return nullptr;
}

CrashSystem collectSystem()
{
    CrashSystem system;
    using RtlGetVersionFn = LONG(WINAPI *)(OSVERSIONINFOW *);
    OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (auto fn = reinterpret_cast<RtlGetVersionFn>(
            reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")))) {
        fn(&version);
    }
    const std::string display =
        registryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion");
    char os[128];
    std::snprintf(os, sizeof(os), "Windows %s %s(build %lu)", version.dwBuildNumber >= 22000 ? "11" : "10",
                  display.empty() ? "" : (display + " ").c_str(), version.dwBuildNumber);
    system.os = os;
    system.cpu = registryString(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                                L"ProcessorNameString");
    while (!system.cpu.empty() && system.cpu.back() == ' ') {
        system.cpu.pop_back();
    }
    system.cores = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        system.ramMb = memory.ullTotalPhys / (1024 * 1024);
    }
    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) > 0) {
        system.locale = narrow(locale);
    }
    return system;
}

CrashProcess collectProcess(HANDLE process, const CrashContext &context)
{
    CrashProcess info;
    const std::uint64_t end = context.crashTick ? context.crashTick : GetTickCount64();
    info.uptimeMs = end > context.startTick ? end - context.startTick : 0;
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(process, &counters, sizeof(counters))) {
        info.peakWorkingSetMb = counters.PeakWorkingSetSize / (1024 * 1024);
    }
    DWORD handles = 0;
    if (GetProcessHandleCount(process, &handles)) {
        info.handles = handles;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        const DWORD pid = GetProcessId(process);
        for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID == pid) {
                ++info.threads;
            }
        }
        CloseHandle(snapshot);
    }
    return info;
}

std::string threadDescription(DWORD threadId)
{
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadId);
    if (!thread) {
        return {};
    }
    PWSTR description = nullptr;
    std::string result;
    if (SUCCEEDED(GetThreadDescription(thread, &description)) && description) {
        result = narrow(description);
        LocalFree(description);
    }
    CloseHandle(thread);
    return result;
}

std::string sha256File(const std::filesystem::path &file)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return {};
    }
    std::string result;
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        std::ifstream input(file, std::ios::binary);
        std::vector<char> buffer(1 << 16);
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto read = static_cast<ULONG>(input.gcount());
            if (read > 0) {
                BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), read, 0);
            }
        }
        unsigned char digest[32];
        if (BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
            char hexDigits[65];
            for (int i = 0; i < 32; ++i) {
                std::snprintf(hexDigits + i * 2, 3, "%02x", digest[i]);
            }
            result.assign(hexDigits, 64);
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return result;
}

}  // namespace sky
