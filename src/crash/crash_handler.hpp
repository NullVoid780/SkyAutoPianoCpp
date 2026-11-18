#pragma once

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
    static void startReporter() noexcept;
    static void breadcrumb(const char *category, const char *message) noexcept;
    static CrashState *state() noexcept;
    static void markCleanExit() noexcept;
};

}  // namespace sky
