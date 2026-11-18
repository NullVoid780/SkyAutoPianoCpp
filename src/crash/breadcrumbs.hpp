#pragma once

#include <string>
#include <vector>

#include "crash/crash_context.hpp"

namespace sky {

void writeBreadcrumb(CrashContext &context, const char *category, const char *message) noexcept;

struct BreadcrumbEntry {
    std::uint64_t timeMs = 0;
    std::string category;
    std::string message;
};

std::vector<BreadcrumbEntry> readBreadcrumbs(const CrashContext &context);

}  // namespace sky
