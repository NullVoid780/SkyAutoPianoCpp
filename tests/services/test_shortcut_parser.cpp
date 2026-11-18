#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "services/settings/shortcut_parser.hpp"
#include "services/settings/shortcut_service.hpp"

using namespace sky;
using Catch::Matchers::WithinAbs;

namespace {

std::uint16_t vk(std::string_view accelerator)
{
    return skyinput::virtualKey(parseShortcut(accelerator).value().key);
}

}  // namespace

TEST_CASE("parses the default shortcuts")
{
    auto play = parseShortcut("Ctrl+Shift+V");
    REQUIRE(play.ok());
    CHECK(skyinput::virtualKey(play.value().key) == 'V');
    CHECK(play.value().modifiers == skyinput::Modifiers{true, false, true, false});

    auto up = parseShortcut("Ctrl+Up");
    REQUIRE(up.ok());
    CHECK(skyinput::virtualKey(up.value().key) == 0x26);
    CHECK(up.value().modifiers.control);
}

TEST_CASE("accepts the tokens the settings window produces")
{
    CHECK(vk("Alt+Num5") == 0x65);
    CHECK(vk("Ctrl+NumAdd") == 0x6B);
    CHECK(vk("Shift+NumDec") == 0x6E);
    CHECK(vk("Win+F12") == 0x7B);
    CHECK(vk("Ctrl+;") == 0xBA);
    CHECK(vk("Ctrl+7") == '7');
    CHECK(vk("Ctrl+PageDown") == 0x22);
    CHECK(vk("control+alt+space") == 0x20);
    CHECK(vk("Ctrl++") == 0xBB);
    CHECK(parseShortcut("Meta+Escape").value().modifiers.win);
}

TEST_CASE("rejects malformed accelerators")
{
    CHECK_FALSE(parseShortcut("").ok());
    CHECK_FALSE(parseShortcut("Ctrl+Shift").ok());
    CHECK_FALSE(parseShortcut("Ctrl+A+B").ok());
    CHECK_FALSE(parseShortcut("Ctrl+Banana").ok());
    CHECK(parseShortcut("Ctrl+Banana").error().code == "SHORTCUT_INVALID");
}

TEST_CASE("speed shortcuts step by 0.1 and clamp to 0.1-5")
{
    CHECK_THAT(ShortcutService::steppedSpeed(1.0, true), WithinAbs(1.1, 1e-12));
    CHECK_THAT(ShortcutService::steppedSpeed(1.0, false), WithinAbs(0.9, 1e-12));
    CHECK(ShortcutService::steppedSpeed(5.0, true) == 5.0);
    CHECK(ShortcutService::steppedSpeed(0.1, false) == 0.1);
    CHECK_THAT(ShortcutService::steppedSpeed(4.95, true), WithinAbs(5.0, 1e-12));
}
