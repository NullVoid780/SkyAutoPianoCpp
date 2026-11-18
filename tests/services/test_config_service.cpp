#include <catch2/catch_test_macros.hpp>

#include <fstream>

#include "core/text_file.hpp"
#include "qt_test_support.hpp"
#include "services/settings/config_service.hpp"

using namespace sky;

namespace {

void writeFile(const std::filesystem::path &file, const std::string &text)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << text;
}

std::string readFile(const std::filesystem::path &file)
{
    return readTextFile(file).value();
}

}  // namespace

TEST_CASE("a missing config is written with the defaults")
{
    ensureCoreApp();
    TempDir dir;
    ConfigService config(dir.path() / "config");
    config.load();
    const std::string text = readFile(dir.path() / "config" / "config.json");
    CHECK(text == dumpJson(ConfigService::defaults(), 4));
    CHECK(text.find("    \"panel\": {\n        \"longPressMode\": false,") != std::string::npos);
    CHECK(config.panel().playbackMode == PlaybackMode::Lite);
    CHECK(config.value().appTheme == AppTheme::Dark);
    CHECK(config.keyboard().keys[9] == ";");
}

TEST_CASE("missing keys are filled recursively and the file is rewritten")
{
    ensureCoreApp();
    TempDir dir;
    const auto file = dir.path() / "config.json";
    writeFile(file, R"({"panel":{"speed":2,"extra":true},"appTheme":"light","custom":1})");
    ConfigService config(dir.path());
    config.load();
    const OrderedJson raw = config.raw();
    CHECK(raw["panel"]["speed"] == 2);
    CHECK(raw["panel"]["extra"] == true);
    CHECK(raw["panel"]["delayNext"] == 1);
    CHECK(raw["custom"] == 1);
    CHECK(raw["shortcut"]["play"] == "Ctrl+Shift+V");
    CHECK(config.value().appTheme == AppTheme::Light);
    CHECK(config.panel().speed == 2.0);
    CHECK(readFile(file) == dumpJson(raw, 4));
}

TEST_CASE("arrays are leaves and are not merged")
{
    ensureCoreApp();
    TempDir dir;
    writeFile(dir.path() / "config.json", R"({"keyboard":{"keys":["a"]}})");
    ConfigService config(dir.path());
    config.load();
    CHECK(config.raw()["keyboard"]["keys"].size() == 1);
    CHECK(config.keyboard().keys[0] == "a");
    CHECK(config.keyboard().keys[1].empty());
}

TEST_CASE("an unreadable config is replaced with the defaults")
{
    ensureCoreApp();
    TempDir dir;
    writeFile(dir.path() / "config.json", "{ broken");
    ConfigService config(dir.path());
    config.load();
    CHECK(config.raw() == ConfigService::defaults());

    writeFile(dir.path() / "config.json", R"({"panel":"oops"})");
    config.load();
    CHECK(config.raw() == ConfigService::defaults());
}

TEST_CASE("mutators persist and notify")
{
    ensureCoreApp();
    TempDir dir;
    ConfigService config(dir.path());
    config.load();
    int notifications = 0;
    QObject::connect(&config, &ConfigService::changed, [&] { ++notifications; });

    config.setTheme(AppTheme::Light);
    config.setTheme(AppTheme::Light);
    CHECK(notifications == 1);

    OrderedJson partial = OrderedJson::object();
    partial["speed"] = 1.5;
    config.updatePanel(partial);
    CHECK(config.panel().speed == 1.5);

    ConfigService reloaded(dir.path());
    reloaded.load();
    CHECK(reloaded.value().appTheme == AppTheme::Light);
    CHECK(reloaded.panel().speed == 1.5);
}

TEST_CASE("safe speed and delay follow the players' rules")
{
    PanelConfig panel;
    panel.speed = -1;
    panel.delayNext = -2;
    CHECK(ConfigService::safeSpeed(panel) == 1.0);
    CHECK(ConfigService::safeDelayNextMs(panel) == 0.0);
    panel.speed = 0.5;
    panel.delayNext = 1.5;
    CHECK(ConfigService::safeSpeed(panel) == 0.5);
    CHECK(ConfigService::safeDelayNextMs(panel) == 1500.0);
}

TEST_CASE("shortcut validation messages match the settings window")
{
    ShortcutConfig shortcuts;
    CHECK(ConfigService::validateShortcuts(shortcuts).ok());

    shortcuts.next = "  ";
    auto empty = ConfigService::validateShortcuts(shortcuts);
    REQUIRE_FALSE(empty.ok());
    CHECK(empty.error().userMessage == "Unable to save the settings, a shortcut is empty!");

    shortcuts.next = "Ctrl+Shift+C";
    auto duplicate = ConfigService::validateShortcuts(shortcuts);
    REQUIRE_FALSE(duplicate.ok());
    CHECK(duplicate.error().userMessage == "Unable to save the settings, the shortcut has been duplicated!");
}

TEST_CASE("shortcut validation compares parsed combinations, not raw text")
{
    ShortcutConfig shortcuts;
    shortcuts.next = "shift+ctrl+c";
    auto reordered = ConfigService::validateShortcuts(shortcuts);
    REQUIRE_FALSE(reordered.ok());
    CHECK(reordered.error().userMessage == "Unable to save the settings, the shortcut has been duplicated!");

    shortcuts.next = "Ctrl+Shift";
    auto modifiersOnly = ConfigService::validateShortcuts(shortcuts);
    REQUIRE_FALSE(modifiersOnly.ok());
    CHECK(modifiersOnly.error().code == "SHORTCUT_INVALID");
}

TEST_CASE("settings files round-trip and reject garbage")
{
    const auto dir = std::filesystem::temp_directory_path() / "sky_settings_file_test";
    std::filesystem::create_directories(dir);
    const auto file = dir / "settings.json";
    OrderedJson config = ConfigService::defaults();
    REQUIRE(ConfigService::writeSettingsFile(file, config).ok());
    auto loaded = ConfigService::readSettingsFile(file);
    REQUIRE(loaded.ok());
    CHECK(loaded.value() == config);

    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "{ not json";
    }
    auto invalid = ConfigService::readSettingsFile(file);
    REQUIRE_FALSE(invalid.ok());
    CHECK(invalid.error().userMessage == "The selected settings file is invalid.");
    std::filesystem::remove_all(dir);
}

TEST_CASE("imported settings only take recognised, in-range values")
{
    const OrderedJson current = ConfigService::defaults();
    const OrderedJson imported = OrderedJson::parse(R"({
        "panel": {"speed": 9, "delayNext": 2, "playbackMode": "strict", "longPressMode": "yes"},
        "keyboard": {"customKeyboard": true, "keys": ["Esc", "PAGEUP", "", 5]},
        "shortcut": {"play": "  Ctrl+P  ", "next": ""},
        "update": {"blockUpdate": true},
        "appTheme": "blue"
    })");
    auto next = ConfigService::normalizeImported(current, imported);
    REQUIRE(next.ok());
    const OrderedJson &config = next.value();
    CHECK(config["panel"]["speed"] == 1);
    CHECK(config["panel"]["delayNext"] == 2);
    CHECK(config["panel"]["playbackMode"] == "strict");
    CHECK(config["panel"]["longPressMode"] == false);
    CHECK(config["keyboard"]["customKeyboard"] == true);
    CHECK(config["keyboard"]["keys"][0] == "escape");
    CHECK(config["keyboard"]["keys"][1] == "pageUp");
    CHECK(config["keyboard"]["keys"][2] == "i");
    CHECK(config["keyboard"]["keys"][3] == "5");
    CHECK(config["keyboard"]["keys"][4] == "p");
    CHECK(config["shortcut"]["play"] == "Ctrl+P");
    CHECK(config["shortcut"]["next"] == "Ctrl+Shift+B");
    CHECK(config["update"]["blockUpdate"] == true);
    CHECK(config["appTheme"] == "dark");

    auto invalid = ConfigService::normalizeImported(current, OrderedJson::array());
    REQUIRE_FALSE(invalid.ok());
    CHECK(invalid.error().userMessage == "The selected settings file is invalid.");
}

TEST_CASE("the lock-free speed follows every config change")
{
    TempDir dir;
    ConfigService config(dir.path());
    config.load();
    CHECK(config.speed() == 1.0);

    OrderedJson faster = OrderedJson::object();
    faster["speed"] = 1.5;
    config.updatePanel(faster);
    CHECK(config.speed() == 1.5);

    OrderedJson replaced = config.raw();
    replaced["panel"]["speed"] = 0;
    REQUIRE(config.replace(replaced).ok());
    CHECK(config.speed() == 1.0);
}
