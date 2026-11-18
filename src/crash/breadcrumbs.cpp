#include "crash/breadcrumbs.hpp"

#include <algorithm>
#include <cstring>

namespace sky {

namespace {

void copyBounded(char *target, std::size_t capacity, const char *source) noexcept
{
    std::size_t i = 0;
    if (source) {
        for (; i + 1 < capacity && source[i] != '\0'; ++i) {
            target[i] = source[i];
        }
    }
    target[i] = '\0';
}

std::string boundedString(const char *text, std::size_t capacity)
{
    const void *end = std::memchr(text, '\0', capacity);
    return std::string(text, end ? static_cast<const char *>(end) - text : capacity);
}

}  // namespace

void writeBreadcrumb(CrashContext &context, const char *category, const char *message) noexcept
{
    const LONG64 sequence = InterlockedIncrement64(&context.breadcrumbNext) - 1;
    Breadcrumb &slot = context.breadcrumbs[sequence % kBreadcrumbCount];
    slot.timeMs = GetTickCount64() - context.startTick;
    copyBounded(slot.category, sizeof(slot.category), category);
    copyBounded(slot.message, sizeof(slot.message), message);
}

std::vector<BreadcrumbEntry> readBreadcrumbs(const CrashContext &context)
{
    const LONG64 written = context.breadcrumbNext;
    const LONG64 count = std::min<LONG64>(written, kBreadcrumbCount);
    std::vector<BreadcrumbEntry> entries;
    entries.reserve(static_cast<std::size_t>(count));
    for (LONG64 i = written - count; i < written; ++i) {
        const Breadcrumb &slot = context.breadcrumbs[i % kBreadcrumbCount];
        entries.push_back(BreadcrumbEntry{slot.timeMs, boundedString(slot.category, sizeof(slot.category)),
                                          boundedString(slot.message, sizeof(slot.message))});
    }
    return entries;
}

}  // namespace sky
