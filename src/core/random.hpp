#pragma once

#include <cstdint>
#include <string>

namespace sky {

double randomUnit();
std::int64_t randomIntInclusive(std::int64_t min, std::int64_t max);
std::string randomBase36(int length);
std::string uuidV4();

}  // namespace sky
