#include "codec/encrypted_notes.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include "core/translator.hpp"
#include "core/string_utils.hpp"

namespace sky {

namespace {

constexpr std::array<int, 32> kMask = {
    16, 34, 56, 18, 62, 19, -25, 55, 15, 24, 30, 12, 30, 45, 39, -23,
    -10, 15, 45, -18, 37, -2, -21, 65, 25, -4, -14, 43, 23, -4, -17, -17,
};

std::int32_t toInt32(double value)
{
    if (!std::isfinite(value)) {
        return 0;
    }
    const double truncated = std::trunc(value);
    const double wrapped = std::fmod(truncated, 4294967296.0);
    const auto bits = static_cast<std::uint32_t>(static_cast<std::int64_t>(wrapped < 0 ? wrapped + 4294967296.0 : wrapped));
    return static_cast<std::int32_t>(bits);
}

bool isLineTerminator(char16_t unit)
{
    return unit == u'\n' || unit == u'\r' || unit == 0x2028 || unit == 0x2029;
}

}  // namespace

bool hasEncryptedNotes(const OrderedJson &rawSheet)
{
    if (!rawSheet.is_object()) {
        return false;
    }
    const auto encrypted = rawSheet.find("isEncrypted");
    if (encrypted != rawSheet.end() && encrypted->is_boolean() && encrypted->get<bool>()) {
        return true;
    }
    const auto notes = rawSheet.find("songNotes");
    return notes != rawSheet.end() && notes->is_array() && !notes->empty() && (*notes)[0].is_number();
}

Result<OrderedJson> decodeEncryptedNotes(const OrderedJson &numbers)
{
    if (!numbers.is_array()) {
        return Error{"DECRYPT_INPUT", trUtf8("Encrypted notes must be an array."), {}};
    }
    std::u16string decoded;
    decoded.reserve(numbers.size());
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        const std::int32_t code = toInt32(jsNumber(numbers[i])) + kMask[i % kMask.size()];
        decoded.push_back(static_cast<char16_t>(static_cast<std::uint32_t>(code) & 0xFFFF));
    }
    const std::size_t bracket = decoded.find(u']');
    if (bracket != std::u16string::npos) {
        std::size_t end = bracket + 1;
        while (end < decoded.size() && !isLineTerminator(decoded[end])) {
            ++end;
        }
        decoded.erase(bracket + 1, end - bracket - 1);
    }
    return parseJson<OrderedJson>(utf16ToUtf8(decoded));
}

}  // namespace sky
