#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "core/base64.hpp"
#include "core/json.hpp"
#include "core/string_utils.hpp"
#include "core/time_utils.hpp"

using namespace sky;

TEST_CASE("base64 matches js-base64 for payload names")
{
    CHECK(base64Encode("123Song-title") == "MTIzU29uZy10aXRsZQ==");
    CHECK(base64Encode("") == "");
    CHECK(base64Encode("ab") == "YWI=");
    CHECK(base64Decode("MTIzU29uZy10aXRsZQ==").value() == "123Song-title");
    CHECK_FALSE(base64Decode("@@").has_value());
}

TEST_CASE("jsNumberFromString follows Number()")
{
    CHECK(jsNumberFromString(" 12 ") == 12.0);
    CHECK(jsNumberFromString("") == 0.0);
    CHECK(jsNumberFromString("0x10") == 16.0);
    CHECK(jsNumberFromString("1e3") == 1000.0);
    CHECK(jsNumberFromString(".5") == 0.5);
    CHECK(jsNumberFromString("-2.5") == -2.5);
    CHECK(std::isinf(jsNumberFromString("Infinity")));
    CHECK(std::isnan(jsNumberFromString("abc")));
    CHECK(std::isnan(jsNumberFromString("1.5.2")));
    CHECK(std::isnan(jsNumberFromString("12px")));
}

TEST_CASE("jsNumber converts JSON values like JavaScript")
{
    CHECK(jsNumber(OrderedJson(true)) == 1.0);
    CHECK(jsNumber(OrderedJson(nullptr)) == 0.0);
    CHECK(jsNumber(OrderedJson("42")) == 42.0);
    CHECK(jsNumber(OrderedJson::array()) == 0.0);
    CHECK(std::isnan(jsNumber(OrderedJson::object())));
    CHECK(std::isnan(jsMember(OrderedJson::object(), "missing")));
}

TEST_CASE("formatJsNumber prints like Number#toString")
{
    CHECK(formatJsNumber(480) == "480");
    CHECK(formatJsNumber(0) == "0");
    CHECK(formatJsNumber(1.5) == "1.5");
    CHECK(formatJsNumber(0.1) == "0.1");
    CHECK(formatJsNumber(-3) == "-3");
}

TEST_CASE("jsonNumber keeps integers integral")
{
    CHECK(dumpJson(jsonNumber(120.0)) == "120");
    CHECK(dumpJson(jsonNumber(68.5)) == "68.5");
}

TEST_CASE("sanitizeUtf8 replaces invalid sequences like the WHATWG decoder")
{
    CHECK(sanitizeUtf8("abc") == "abc");
    CHECK(sanitizeUtf8("\xC3\x28") == "\xEF\xBF\xBD(");
    CHECK(sanitizeUtf8("\xE2\x82") == "\xEF\xBF\xBD");
    CHECK(sanitizeUtf8("\xF0\x9F\x8E\xB9") == "\xF0\x9F\x8E\xB9");
}

TEST_CASE("UTF-8 and UTF-16 round trip")
{
    const std::string text = "Sky \xE2\x99\xAA \xF0\x9F\x8E\xB9";
    CHECK(utf16ToUtf8(utf8ToUtf16(text)) == text);
    CHECK(fromWide(toWide(text)) == text);
    CHECK(utf16Length(text) == 8);
    CHECK(utf16ToUtf8(std::u16string(1, char16_t(0xD800))) == "\xEF\xBF\xBD");
}

TEST_CASE("trimJs and splitOnJsWhitespace follow String#trim and split(/\\s+/)")
{
    CHECK(trimJs("\xC2\xA0 hi \xEF\xBB\xBF") == "hi");
    CHECK(trimJs("   ") == "");
    const auto parts = splitOnJsWhitespace("  a \t b\xE3\x80\x80" "c  ");
    REQUIRE(parts.size() == 3);
    CHECK(parts[0] == "a");
    CHECK(parts[1] == "b");
    CHECK(parts[2] == "c");
    const auto empty = splitOnJsWhitespace("   ");
    REQUIRE(empty.size() == 1);
    CHECK(empty[0].empty());
}

TEST_CASE("ascii helpers")
{
    CHECK(toLowerAscii("MiXeD") == "mixed");
    CHECK(equalsIgnoreCaseAscii("PageUp", "pageup"));
    CHECK(startsWith("<DontCopyThisLine> 1", "<DontCopyThisLine>"));
    CHECK(endsWith("song.json", ".json"));
}

TEST_CASE("formatClock pads minutes and seconds")
{
    CHECK(formatClock(0) == "00:00");
    CHECK(formatClock(75) == "01:15");
    CHECK(formatClock(-4) == "00:00");
}
