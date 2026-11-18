#include "core/base64.hpp"

#include <array>
#include <cstdint>

namespace sky {

namespace {

constexpr std::string_view kAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int decodeChar(char c)
{
    const auto pos = kAlphabet.find(c);
    return pos == std::string_view::npos ? -1 : static_cast<int>(pos);
}

}  // namespace

std::string base64Encode(std::string_view bytes)
{
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3) {
        const std::uint32_t chunk = (static_cast<unsigned char>(bytes[i]) << 16) |
                                    (static_cast<unsigned char>(bytes[i + 1]) << 8) |
                                    static_cast<unsigned char>(bytes[i + 2]);
        out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
        out.push_back(kAlphabet[chunk & 0x3F]);
    }
    const std::size_t rest = bytes.size() - i;
    if (rest == 1) {
        const std::uint32_t chunk = static_cast<unsigned char>(bytes[i]) << 16;
        out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
        out.append("==");
    } else if (rest == 2) {
        const std::uint32_t chunk = (static_cast<unsigned char>(bytes[i]) << 16) |
                                    (static_cast<unsigned char>(bytes[i + 1]) << 8);
        out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

std::optional<std::string> base64Decode(std::string_view text)
{
    std::string out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : text) {
        if (c == '=') {
            break;
        }
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
            continue;
        }
        const int value = decodeChar(c);
        if (value < 0) {
            return std::nullopt;
        }
        buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

}  // namespace sky
