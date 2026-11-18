#include "crash/minidump_writer.hpp"

#include <dbghelp.h>

namespace sky {

bool writeMinidump(HANDLE process, DWORD processId, DWORD threadId, EXCEPTION_POINTERS *pointers,
                   bool clientPointers, const std::filesystem::path &file) noexcept
{
    HANDLE output = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        return false;
    }
    MINIDUMP_EXCEPTION_INFORMATION exception{};
    exception.ThreadId = threadId;
    exception.ExceptionPointers = pointers;
    exception.ClientPointers = clientPointers ? TRUE : FALSE;
    const auto flags = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithUnloadedModules |
        MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory | MiniDumpWithProcessThreadData |
        MiniDumpWithThreadInfo);
    const BOOL ok = MiniDumpWriteDump(process, processId, output, flags, pointers ? &exception : nullptr, nullptr,
                                      nullptr);
    CloseHandle(output);
    if (!ok) {
        DeleteFileW(file.c_str());
    }
    return ok != FALSE;
}

}  // namespace sky
