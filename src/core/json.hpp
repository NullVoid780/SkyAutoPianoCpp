#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "core/result.hpp"
#include "core/string_utils.hpp"

namespace sky {

using Json = nlohmann::json;
using OrderedJson = nlohmann::ordered_json;

inline constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

inline double jsNumberFromString(std::string_view raw)
{
    const std::string_view text = trimJs(raw);
    if (text.empty()) {
        return 0.0;
    }
    if (text == "Infinity" || text == "+Infinity") {
        return std::numeric_limits<double>::infinity();
    }
    if (text == "-Infinity") {
        return -std::numeric_limits<double>::infinity();
    }
    if (text.size() > 2 && text[0] == '0') {
        int base = 0;
        const char marker = text[1];
        if (marker == 'x' || marker == 'X') {
            base = 16;
        } else if (marker == 'o' || marker == 'O') {
            base = 8;
        } else if (marker == 'b' || marker == 'B') {
            base = 2;
        }
        if (base != 0) {
            double value = 0.0;
            for (char c : text.substr(2)) {
                int digit = -1;
                if (c >= '0' && c <= '9') {
                    digit = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    digit = c - 'a' + 10;
                } else if (c >= 'A' && c <= 'F') {
                    digit = c - 'A' + 10;
                }
                if (digit < 0 || digit >= base) {
                    return kNaN;
                }
                value = value * base + digit;
            }
            return value;
        }
    }

    std::size_t pos = 0;
    bool negative = false;
    if (text[pos] == '+' || text[pos] == '-') {
        negative = text[pos] == '-';
        ++pos;
    }
    const std::size_t mantissaStart = pos;
    bool digits = false;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
        ++pos;
        digits = true;
    }
    if (pos < text.size() && text[pos] == '.') {
        ++pos;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            ++pos;
            digits = true;
        }
    }
    if (!digits) {
        return kNaN;
    }
    if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
        ++pos;
        if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
            ++pos;
        }
        const std::size_t expStart = pos;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            ++pos;
        }
        if (pos == expStart) {
            return kNaN;
        }
    }
    if (pos != text.size()) {
        return kNaN;
    }
    std::string literal(text.substr(mantissaStart));
    if (literal.front() == '.') {
        literal.insert(literal.begin(), '0');
    }
    double value = 0.0;
    const auto result = std::from_chars(literal.data(), literal.data() + literal.size(), value);
    if (result.ec == std::errc::result_out_of_range) {
        value = std::numeric_limits<double>::infinity();
    } else if (result.ec != std::errc()) {
        return kNaN;
    }
    return negative ? -value : value;
}

template <typename JsonType>
double jsNumber(const JsonType &value)
{
    if (value.is_number()) {
        return value.template get<double>();
    }
    if (value.is_boolean()) {
        return value.template get<bool>() ? 1.0 : 0.0;
    }
    if (value.is_null()) {
        return 0.0;
    }
    if (value.is_string()) {
        return jsNumberFromString(value.template get_ref<const std::string &>());
    }
    if (value.is_array()) {
        if (value.empty()) {
            return 0.0;
        }
        if (value.size() == 1 && (value[0].is_number() || value[0].is_string())) {
            return jsNumber(value[0]);
        }
    }
    return kNaN;
}

template <typename JsonType>
double jsMember(const JsonType &object, const char *key)
{
    if (!object.is_object()) {
        return kNaN;
    }
    const auto it = object.find(key);
    return it == object.end() ? kNaN : jsNumber(*it);
}

template <typename JsonType>
bool jsTruthy(const JsonType &value)
{
    if (value.is_null() || value.is_discarded()) {
        return false;
    }
    if (value.is_boolean()) {
        return value.template get<bool>();
    }
    if (value.is_number()) {
        const double n = value.template get<double>();
        return n != 0.0 && !std::isnan(n);
    }
    if (value.is_string()) {
        return !value.template get_ref<const std::string &>().empty();
    }
    return true;
}

inline std::string formatJsNumber(double value)
{
    if (std::isnan(value)) {
        return "NaN";
    }
    if (std::isinf(value)) {
        return value > 0 ? "Infinity" : "-Infinity";
    }
    if (value == 0.0) {
        return "0";
    }
    if (std::trunc(value) == value && std::fabs(value) < 1e21) {
        char buffer[32];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, 0);
        return std::string(buffer, result.ptr);
    }
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

inline OrderedJson jsonNumber(double value)
{
    if (std::trunc(value) == value && std::fabs(value) < 9.007199254740992e15) {
        return static_cast<std::int64_t>(value);
    }
    return value;
}

template <typename JsonType>
std::string dumpJson(const JsonType &value, int indent = -1)
{
    return value.dump(indent, ' ', false, nlohmann::detail::error_handler_t::replace);
}

template <typename JsonType>
Result<JsonType> parseJson(std::string_view text)
{
    try {
        return JsonType::parse(text.begin(), text.end());
    } catch (const nlohmann::json::exception &e) {
        return Error{"INVALID_JSON", e.what(), {}};
    }
}

}  // namespace sky
