#pragma once

#include <vector>

#include "crash/breadcrumbs.hpp"
#include "crash/crash_context.hpp"

namespace sky {

class CrashHandler {
public:
    using PanicRelease = unsigned (*)() noexcept;

    struct Options {
        const char *appVersion = "";
        PanicRelease panicRelease = nullptr;
    };

    static bool install(const Options &options);
    static void setLanguage(const char *code) noexcept;
    static void startReporter() noexcept;
    static void breadcrumb(const char *category, const char *message) noexcept;
    static CrashState *state() noexcept;
    static std::vector<BreadcrumbEntry> recentBreadcrumbs();
    static void markCleanExit() noexcept;
};

}  // namespace sky
