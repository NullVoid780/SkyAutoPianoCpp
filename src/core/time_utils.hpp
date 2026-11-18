#pragma once

#include <cstdint>
#include <string>

namespace sky {

std::int64_t epochMillis();
std::int64_t steadyMillis();
std::string isoTimestampUtc();
std::string compactLocalTimestamp();
std::string formatClock(int totalSeconds);

}  // namespace sky
