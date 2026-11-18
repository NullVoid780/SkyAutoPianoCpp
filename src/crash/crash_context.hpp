#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>

namespace sky {

inline constexpr std::uint32_t kCrashMagic = 0x43594B53;
inline constexpr std::uint32_t kCrashLayoutVersion = 1;
inline constexpr int kBreadcrumbCount = 128;
inline constexpr int kBreadcrumbCategoryBytes = 16;
inline constexpr int kBreadcrumbMessageBytes = 104;

enum class CrashReason : std::uint32_t {
    None = 0,
    SehException,
    CppTerminate,
    Abort,
    InvalidParameter,
    PureCall,
    OutOfMemory,
};

inline constexpr DWORD kTerminateCode = 0xE0000101;
inline constexpr DWORD kAbortCode = 0xE0000102;
inline constexpr DWORD kInvalidParameterCode = 0xE0000103;
inline constexpr DWORD kPureCallCode = 0xE0000104;
inline constexpr DWORD kOutOfMemoryCode = 0xE0000105;

struct Breadcrumb {
    std::uint64_t timeMs;
    char category[kBreadcrumbCategoryBytes];
    char message[kBreadcrumbMessageBytes];
};
static_assert(sizeof(Breadcrumb) == 128);

struct CrashState {
    std::int32_t playbackMode;
    std::int32_t playing;
    double speed;
    std::int32_t longPress;
    std::int32_t theme;
    std::int32_t customKeyboard;
    std::int32_t librarySize;
    std::int32_t noteCount;
    std::int32_t reserved;
};

struct CrashContext {
    std::uint32_t magic;
    std::uint32_t layoutVersion;
    std::uint32_t processId;
    std::uint32_t threadId;
    std::uint64_t exceptionPointers;
    std::uint32_t reason;
    std::uint32_t exceptionCode;
    std::uint64_t exceptionAddress;
    std::uint64_t startTick;
    std::uint64_t crashTick;
    volatile LONG cleanExit;
    volatile LONG crashed;
    char appVersion[32];
    CrashState state;
    volatile LONG64 breadcrumbNext;
    Breadcrumb breadcrumbs[kBreadcrumbCount];
    EXCEPTION_RECORD record;
    alignas(16) CONTEXT context;
};

}  // namespace sky
