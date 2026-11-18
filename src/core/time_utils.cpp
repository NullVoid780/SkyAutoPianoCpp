#include "core/time_utils.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace sky {

namespace {

std::tm toTm(std::time_t seconds, bool utc)
{
    std::tm parts{};
    if (utc) {
        gmtime_s(&parts, &seconds);
    } else {
        localtime_s(&parts, &seconds);
    }
    return parts;
}

}  // namespace

std::int64_t epochMillis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::int64_t steadyMillis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string isoTimestampUtc()
{
    const std::int64_t millis = epochMillis();
    const std::tm parts = toTm(static_cast<std::time_t>(millis / 1000), true);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", parts.tm_year + 1900,
                  parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec,
                  static_cast<int>(millis % 1000));
    return buffer;
}

std::string compactLocalTimestamp()
{
    const std::tm parts = toTm(static_cast<std::time_t>(epochMillis() / 1000), false);
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d-%02d%02d%02d", parts.tm_year + 1900,
                  parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
    return buffer;
}

std::string formatClock(int totalSeconds)
{
    if (totalSeconds < 0) {
        totalSeconds = 0;
    }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", totalSeconds / 60, totalSeconds % 60);
    return buffer;
}

}  // namespace sky
