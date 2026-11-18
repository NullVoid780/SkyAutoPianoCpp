#include "core/random.hpp"

#include <cstdio>
#include <random>

namespace sky {

namespace {

std::mt19937_64 &engine()
{
    thread_local std::mt19937_64 generator{std::random_device{}()};
    return generator;
}

}  // namespace

double randomUnit()
{
    return std::uniform_real_distribution<double>(0.0, 1.0)(engine());
}

std::int64_t randomIntInclusive(std::int64_t min, std::int64_t max)
{
    return std::uniform_int_distribution<std::int64_t>(min, max)(engine());
}

std::string randomBase36(int length)
{
    static constexpr char kDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string out;
    for (int i = 0; i < length; ++i) {
        out.push_back(kDigits[randomIntInclusive(0, 35)]);
    }
    return out;
}

std::string uuidV4()
{
    std::uint64_t high = engine()();
    std::uint64_t low = engine()();
    high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<unsigned>(high >> 32), static_cast<unsigned>((high >> 16) & 0xFFFF),
                  static_cast<unsigned>(high & 0xFFFF), static_cast<unsigned>(low >> 48),
                  static_cast<unsigned long long>(low & 0xFFFFFFFFFFFFULL));
    return buffer;
}

}  // namespace sky
