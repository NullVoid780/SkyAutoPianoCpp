#pragma once

#include <cstddef>
#include <initializer_list>
#include <string_view>
#include <vector>

inline std::vector<std::byte> bytesOf(std::string_view text)
{
    std::vector<std::byte> bytes(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        bytes[i] = static_cast<std::byte>(static_cast<unsigned char>(text[i]));
    }
    return bytes;
}

inline std::vector<std::byte> bytesOf(std::initializer_list<int> values)
{
    std::vector<std::byte> bytes;
    bytes.reserve(values.size());
    for (int value : values) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

inline std::vector<std::byte> utf16le(std::string_view ascii, bool withBom)
{
    std::vector<std::byte> bytes;
    if (withBom) {
        bytes.push_back(std::byte{0xFF});
        bytes.push_back(std::byte{0xFE});
    }
    for (char c : ascii) {
        bytes.push_back(static_cast<std::byte>(c));
        bytes.push_back(std::byte{0});
    }
    return bytes;
}
