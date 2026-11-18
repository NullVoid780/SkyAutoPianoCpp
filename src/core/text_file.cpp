#include "core/text_file.hpp"

#include "core/file_system.hpp"
#include "core/string_utils.hpp"

namespace sky {

namespace {

std::string decodeUtf16(std::string_view bytes, bool bigEndian)
{
    std::u16string units;
    units.reserve(bytes.size() / 2);
    for (std::size_t i = 0; i + 1 < bytes.size(); i += 2) {
        const auto first = static_cast<unsigned char>(bytes[i]);
        const auto second = static_cast<unsigned char>(bytes[i + 1]);
        units.push_back(static_cast<char16_t>(bigEndian ? (first << 8) | second : (second << 8) | first));
    }
    return utf16ToUtf8(units);
}

}  // namespace

std::string decodeTextBytes(std::string_view bytes)
{
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        return decodeUtf16(bytes.substr(2), false);
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        return decodeUtf16(bytes.substr(2), true);
    }
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        return sanitizeUtf8(bytes.substr(3));
    }
    return sanitizeUtf8(bytes);
}

Result<std::string> readTextFile(const std::filesystem::path &file)
{
    auto bytes = readFileBytes(file);
    if (!bytes) {
        return bytes.error();
    }
    const auto &data = bytes.value();
    return decodeTextBytes(std::string_view(reinterpret_cast<const char *>(data.data()), data.size()));
}

}  // namespace sky
