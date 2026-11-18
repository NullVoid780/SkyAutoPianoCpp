#pragma once

#include <string>
#include <vector>

#include "crash/crash_context.hpp"

namespace sky {

std::vector<std::string> walkStack(HANDLE process, DWORD threadId, const CONTEXT &context, int maxFrames = 64);

}  // namespace sky
