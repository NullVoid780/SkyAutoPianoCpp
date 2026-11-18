#include "codec/text_decoder.hpp"

#include <algorithm>
#include <string_view>

#include "core/string_utils.hpp"

namespace sky {

namespace {

constexpr std::size_t kSampleLength = 200;

unsigned char byteAt(std::span<const std::byte> content, std::size_t index)
{
    return static_cast<unsigned char>(content[index]);
}

std::string decodeUtf16(std::span<const std::byte> bytes, bool swapped)
{
    std::u16string units;
    units.reserve(bytes.size() / 2);
    for (std::size_t i = 0; i + 1 < bytes.size(); i += 2) {
        const auto first = byteAt(bytes, i);
        const auto second = byteAt(bytes, i + 1);
        units.push_back(static_cast<char16_t>(swapped ? (first << 8) | second : (second << 8) | first));
    }
    return utf16ToUtf8(units);
}

std::string decodeUtf8(std::span<const std::byte> bytes)
{
    return sanitizeUtf8(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}

}  // namespace

std::string decodeSheetText(std::span<const std::byte> content)
{
    if (content.size() >= 2 && byteAt(content, 0) == 0xFF && byteAt(content, 1) == 0xFE) {
        return decodeUtf16(content.subspan(2), false);
    }
    if (content.size() >= 2 && byteAt(content, 0) == 0xFE && byteAt(content, 1) == 0xFF) {
        return decodeUtf16(content.subspan(2), true);
    }
    if (content.size() >= 3 && byteAt(content, 0) == 0xEF && byteAt(content, 1) == 0xBB &&
        byteAt(content, 2) == 0xBF) {
        return decodeUtf8(content.subspan(3));
    }

    const std::size_t sampleLength = std::min(content.size(), kSampleLength);
    std::size_t oddZeros = 0;
    std::size_t evenZeros = 0;
    for (std::size_t i = 0; i < sampleLength; ++i) {
        if (byteAt(content, i) != 0) {
            continue;
        }
        if (i % 2) {
            ++oddZeros;
        } else {
            ++evenZeros;
        }
    }
    const double threshold = static_cast<double>(sampleLength) / 8.0;
    if (static_cast<double>(oddZeros) > threshold) {
        return decodeUtf16(content, false);
    }
    if (static_cast<double>(evenZeros) > threshold) {
        return decodeUtf16(content, true);
    }
    return decodeUtf8(content);
}

}  // namespace sky
