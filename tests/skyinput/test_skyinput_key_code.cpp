#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>

#include "skyinput/key_code.hpp"

using namespace skyinput;

TEST_CASE("maps the keysender names to virtual keys")
{
    CHECK(virtualKey(*keyFromName("y")) == 0x59);
    CHECK(virtualKey(*keyFromName(";")) == 0xBA);
    CHECK(virtualKey(*keyFromName(",")) == 0xBC);
    CHECK(virtualKey(*keyFromName(".")) == 0xBE);
    CHECK(virtualKey(*keyFromName("/")) == 0xBF);
    CHECK(virtualKey(*keyFromName("num+")) == 0x6B);
    CHECK(virtualKey(*keyFromName("pageUp")) == 0x21);
    CHECK(virtualKey(*keyFromName("escape")) == 0x1B);
    CHECK(virtualKey(*keyFromName("f24")) == 0x87);
    CHECK_FALSE(keyFromName("esc").has_value());
    CHECK_FALSE(keyFromName("PageUp").has_value());
}

TEST_CASE("every name round-trips and names are unique")
{
    std::set<std::string> seen;
    for (std::string_view name : keyNames()) {
        CHECK(seen.insert(std::string(name)).second);
        const auto key = keyFromName(name);
        REQUIRE(key.has_value());
        CHECK(nameFromKey(*key) == name);
    }
    CHECK(nameFromKey(keyCodeFromVirtualKey(0xFF)).empty());
}

TEST_CASE("the extended key set matches the addon")
{
    CHECK(isExtendedKey(*keyFromName("up")));
    CHECK(isExtendedKey(*keyFromName("rCtrl")));
    CHECK(isExtendedKey(*keyFromName("delete")));
    CHECK_FALSE(isExtendedKey(*keyFromName("y")));
    CHECK_FALSE(isExtendedKey(*keyFromName("num+")));
    CHECK_FALSE(isExtendedKey(*keyFromName("lCtrl")));
}

TEST_CASE("printable keys have a scan code")
{
    for (std::string_view name : {"y", "u", "i", "o", "p", "h", "j", "k", "l", ";", "n", "m", ",", ".", "/"}) {
        CHECK(scanCodeOf(*keyFromName(name)) != 0);
    }
}
