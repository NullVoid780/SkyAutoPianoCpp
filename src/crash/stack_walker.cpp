#include "crash/stack_walker.hpp"

#include <dbghelp.h>

#include <cstdio>
#include <filesystem>

namespace sky {

namespace {

std::string moduleName(HANDLE process, DWORD64 address)
{
    IMAGEHLP_MODULEW64 module{};
    module.SizeOfStruct = sizeof(module);
    if (!SymGetModuleInfoW64(process, address, &module)) {
        return "?";
    }
    return std::filesystem::path(module.ImageName).filename().string();
}

}  // namespace

std::vector<std::string> walkStack(HANDLE process, DWORD threadId, const CONTEXT &context, int maxFrames)
{
    std::vector<std::string> frames;
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_NO_PROMPTS);
    if (!SymInitializeW(process, nullptr, TRUE)) {
        return frames;
    }
    HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, threadId);
    CONTEXT walk = context;
    STACKFRAME64 frame{};
#if defined(_M_X64)
    const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = walk.Rip;
    frame.AddrFrame.Offset = walk.Rbp;
    frame.AddrStack.Offset = walk.Rsp;
#else
    const DWORD machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = walk.Eip;
    frame.AddrFrame.Offset = walk.Ebp;
    frame.AddrStack.Offset = walk.Esp;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    for (int i = 0; i < maxFrames; ++i) {
        if (!StackWalk64(machine, process, thread ? thread : GetCurrentThread(), &frame, &walk, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }
        const DWORD64 address = frame.AddrPC.Offset;
        auto *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
        ZeroMemory(buffer, sizeof(buffer));
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        char line[512];
        const std::string module = moduleName(process, address);
        if (SymFromAddr(process, address, &displacement, symbol)) {
            std::snprintf(line, sizeof(line), "%s!%s+0x%llx", module.c_str(), symbol->Name,
                          static_cast<unsigned long long>(displacement));
        } else {
            const DWORD64 base = SymGetModuleBase64(process, address);
            std::snprintf(line, sizeof(line), "%s+0x%llx", module.c_str(),
                          static_cast<unsigned long long>(base ? address - base : address));
        }
        frames.emplace_back(line);
    }
    if (thread) {
        CloseHandle(thread);
    }
    SymCleanup(process);
    return frames;
}

}  // namespace sky
