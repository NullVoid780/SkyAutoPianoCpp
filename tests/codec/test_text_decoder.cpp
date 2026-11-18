#include <catch2/catch_test_macros.hpp>

#include "codec/text_decoder.hpp"
#include "test_helpers.hpp"

using namespace sky;

TEST_CASE("decodes byte-order marks")
{
    CHECK(decodeSheetText(bytesOf("\xEF\xBB\xBFhello")) == "hello");
    CHECK(decodeSheetText(utf16le("hi", true)) == "hi");
    CHECK(decodeSheetText(bytesOf({0xFE, 0xFF, 0x00, 'h', 0x00, 'i'})) == "hi");
}

TEST_CASE("detects BOM-less UTF-16 from zero bytes")
{
    CHECK(decodeSheetText(utf16le("[{\"name\":\"x\"}]", false)) == "[{\"name\":\"x\"}]");
    CHECK(decodeSheetText(bytesOf({0x00, 'a', 0x00, 'b', 0x00, 'c'})) == "abc");
}

TEST_CASE("falls back to UTF-8 with replacement characters")
{
    CHECK(decodeSheetText(bytesOf("plain")) == "plain");
    CHECK(decodeSheetText(bytesOf("bad\xFF")) == "bad\xEF\xBF\xBD");
    CHECK(decodeSheetText({}) == "");
}

TEST_CASE("an odd trailing UTF-16 byte is dropped")
{
    CHECK(decodeSheetText(bytesOf({0xFF, 0xFE, 'o', 0x00, 'k'})) == "o");
}
