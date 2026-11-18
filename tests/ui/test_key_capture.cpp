#include <catch2/catch_test_macros.hpp>

#include "services/settings/shortcut_parser.hpp"
#include "ui/settings_window/key_capture.hpp"

using namespace sky;

TEST_CASE("keyboard mapping keys follow the alias table")
{
    const auto letter = keyboardKeyForVirtualKey(0x41);
    REQUIRE(letter);
    CHECK(letter->value == "a");
    CHECK(letter->label == "A");

    const auto numAdd = keyboardKeyForVirtualKey(0x6B);
    REQUIRE(numAdd);
    CHECK(numAdd->value == "num+");
    CHECK(numAdd->label == "N+");

    const auto pageUp = keyboardKeyForVirtualKey(0x21);
    REQUIRE(pageUp);
    CHECK(pageUp->value == "pageUp");
    CHECK(pageUp->label == "PgU");

    CHECK(keyboardKeyForVirtualKey(0x1B)->label == "Esc");
    CHECK(keyboardKeyForVirtualKey(0x63)->value == "num3");
    CHECK_FALSE(keyboardKeyForVirtualKey(0x09));
    CHECK_FALSE(keyboardKeyForVirtualKey(0x70));
}

TEST_CASE("stored keyboard values show their short labels, legacy names included")
{
    CHECK(keyboardDisplayLabel("y") == "Y");
    CHECK(keyboardDisplayLabel("esc") == "Esc");
    CHECK(keyboardDisplayLabel("pagedown") == "PgD");
    CHECK(keyboardDisplayLabel("numadd") == "N+");
    CHECK(keyboardDisplayLabel("num7") == "N7");
    CHECK(keyboardDisplayLabel(";") == ";");
    CHECK(keyboardDisplayLabel("") == "");
}

TEST_CASE("shortcut tokens use the original naming")
{
    CHECK(shortcutTokenForVirtualKey(0xA2) == "Ctrl");
    CHECK(shortcutTokenForVirtualKey(0xA5) == "Alt");
    CHECK(shortcutTokenForVirtualKey(0x5C) == "Win");
    CHECK(shortcutTokenForVirtualKey(0x43) == "C");
    CHECK(shortcutTokenForVirtualKey(0x64) == "Num4");
    CHECK(shortcutTokenForVirtualKey(0x6D) == "NumSub");
    CHECK(shortcutTokenForVirtualKey(0x26) == "Up");
    CHECK(shortcutTokenForVirtualKey(0x74) == "F5");
    CHECK(shortcutTokenForVirtualKey(0xBA) == ";");
    CHECK(shortcutTokenForVirtualKey(0xFF).isEmpty());
}

TEST_CASE("formatted shortcuts put modifiers first, then the key")
{
    CHECK(formatShortcut({"C", "Shift", "Ctrl"}) == "Ctrl+Shift+C");
    CHECK(formatShortcut({"Up", "Ctrl"}) == "Ctrl+Up");
    CHECK(formatShortcut({"Shift", "F5"}) == "Shift+F5");
    CHECK(formatShortcut({"NumAdd", "Shift"}) == "Shift+NumAdd");
    CHECK(formatShortcut({"Win", "Alt", "X"}) == "Alt+Win+X");
    CHECK(hasMainKey({"Ctrl", "X"}));
    CHECK_FALSE(hasMainKey({"Ctrl", "Shift"}));
}

TEST_CASE("every captured shortcut token can be registered")
{
    for (quint32 vk = 0x08; vk <= 0xDE; ++vk) {
        const QString token = shortcutTokenForVirtualKey(vk);
        if (token.isEmpty() || isModifierToken(token)) {
            continue;
        }
        const QString combo = formatShortcut({QStringLiteral("Ctrl"), token});
        INFO(combo.toStdString());
        CHECK(parseShortcut(combo.toStdString()).ok());
    }
}
