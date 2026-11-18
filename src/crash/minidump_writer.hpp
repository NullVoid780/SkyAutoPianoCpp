#pragma once

#include <filesystem>

#include "crash/crash_context.hpp"

namespace sky {

bool writeMinidump(HANDLE process, DWORD processId, DWORD threadId, EXCEPTION_POINTERS *pointers,
                   bool clientPointers, const std::filesystem::path &file) noexcept;

}  // namespace sky
