#include <catch2/catch_test_macros.hpp>

#include "core/file_system.hpp"
#include "core/text_file.hpp"
#include "qt_test_support.hpp"
#include "services/settings/theme_library_service.hpp"
#include "services/settings/ui_state_service.hpp"

using namespace sky;

namespace {

CustomTheme sample(const std::string &name)
{
    CustomTheme theme;
    theme.name = name;
    theme.light = {{"card.fill", "#fafafa"}};
    theme.dark = {{"card.fill", "#112233"}, {"footer.played", "#ff880080"}};
    return theme;
}

}  // namespace

TEST_CASE("custom themes save, reload and delete")
{
    ensureCoreApp();
    TempDir dir;
    std::string id;
    {
        ThemeLibraryService library(dir.path());
        library.load();
        CHECK(library.themes().empty());
        auto saved = library.save(sample("  Night   Owl "));
        REQUIRE(saved);
        id = saved.value();
        CHECK(id.rfind("theme-", 0) == 0);
    }
    ThemeLibraryService reloaded(dir.path());
    reloaded.load();
    REQUIRE(reloaded.themes().size() == 1);
    const CustomTheme *theme = reloaded.find(id);
    REQUIRE(theme);
    CHECK(theme->name == "Night Owl");
    REQUIRE(theme->light.size() == 1);
    REQUIRE(theme->dark.size() == 2);
    CHECK(theme->dark[1].second == "#ff880080");

    CHECK(reloaded.remove(id));
    CHECK_FALSE(reloaded.remove(id));
    CHECK(reloaded.themes().empty());
}

TEST_CASE("theme names are required and kept unique")
{
    ensureCoreApp();
    TempDir dir;
    ThemeLibraryService library(dir.path());
    library.load();
    CHECK_FALSE(library.save(sample("   ")));
    auto first = library.save(sample("Ocean"));
    auto second = library.save(sample("ocean"));
    REQUIRE(first);
    REQUIRE(second);
    CHECK(library.find(second.value())->name == "ocean 2");

    CustomTheme renamed = *library.find(first.value());
    renamed.name = "Ocean";
    REQUIRE(library.save(renamed));
    CHECK(library.find(first.value())->name == "Ocean");
    CHECK(library.themes().size() == 2);
}

TEST_CASE("bad color values and keys are dropped")
{
    ensureCoreApp();
    TempDir dir;
    ThemeLibraryService library(dir.path());
    library.load();
    CustomTheme theme = sample("Mixed");
    theme.dark = {{"card.fill", "#12345"}, {"bad key!", "#ffffff"}, {"footer.fill", "#ABCDEF"}, {"app.text", "red"}};
    auto saved = library.save(theme);
    REQUIRE(saved);
    REQUIRE(library.find(saved.value())->dark.size() == 1);
    CHECK(library.find(saved.value())->dark[0].first == "footer.fill");
}

TEST_CASE("theme files export and import without the library id")
{
    ensureCoreApp();
    TempDir dir;
    CustomTheme theme = sample("Shared");
    theme.id = "theme-1-abc";
    const auto file = dir.path() / "shared-theme.json";
    REQUIRE(ThemeLibraryService::writeThemeFile(file, theme));
    const auto text = readTextFile(file);
    REQUIRE(text);
    CHECK(text.value().find("sky-auto-piano-theme") != std::string::npos);
    CHECK(text.value().find("theme-1-abc") == std::string::npos);

    auto imported = ThemeLibraryService::readThemeFile(file);
    REQUIRE(imported);
    CHECK(imported.value().id.empty());
    CHECK(imported.value().name == "Shared");
    CHECK(imported.value().light.size() == 1);
    CHECK(imported.value().dark.size() == 2);
}

TEST_CASE("theme files from the single-mode format load into the right mode")
{
    ensureCoreApp();
    TempDir dir;
    const auto file = dir.path() / "old-theme.json";
    REQUIRE(atomicWrite(file, R"({"format": "sky-auto-piano-theme", "version": 1, "name": "Old", "base": "dark",
                                  "colors": {"card.fill": "#123456"}})"));
    auto imported = ThemeLibraryService::readThemeFile(file);
    REQUIRE(imported);
    CHECK(imported.value().light.empty());
    REQUIRE(imported.value().dark.size() == 1);
    CHECK(imported.value().dark[0].second == "#123456");
}

TEST_CASE("files that are not theme files are rejected")
{
    ensureCoreApp();
    TempDir dir;
    const auto file = dir.path() / "settings.json";
    REQUIRE(atomicWrite(file, R"({"panel": {"speed": 1}})"));
    CHECK_FALSE(ThemeLibraryService::readThemeFile(file));
    REQUIRE(atomicWrite(file, R"({"format": "sky-auto-piano-theme", "name": "X", "base": "blue", "colors": {}})"));
    CHECK_FALSE(ThemeLibraryService::readThemeFile(file));
}

TEST_CASE("ui state remembers the chosen theme apart from light or dark")
{
    ensureCoreApp();
    TempDir dir;
    {
        UiStateService state(dir.path());
        state.load();
        CHECK(state.themeId().empty());
        state.setThemeId("builtin-sakura");
        state.setTheme(AppTheme::Dark);
    }
    UiStateService reloaded(dir.path());
    reloaded.load();
    CHECK(reloaded.themeId() == "builtin-sakura");
    CHECK(reloaded.theme() == AppTheme::Dark);
    reloaded.setThemeId("");
    CHECK(reloaded.themeId().empty());
}

TEST_CASE("following the Windows theme is on by default and is remembered")
{
    ensureCoreApp();
    TempDir dir;
    {
        UiStateService state(dir.path());
        state.load();
        CHECK(state.followSystemTheme());
        state.setFollowSystemTheme(false);
    }
    UiStateService reloaded(dir.path());
    reloaded.load();
    CHECK_FALSE(reloaded.followSystemTheme());
    reloaded.setFollowSystemTheme(true);
    CHECK(reloaded.followSystemTheme());
}

TEST_CASE("quick start shows once and is remembered")
{
    ensureCoreApp();
    TempDir dir;
    {
        UiStateService state(dir.path());
        state.load();
        CHECK_FALSE(state.quickStartDone());
        state.setQuickStartDone();
    }
    UiStateService reloaded(dir.path());
    reloaded.load();
    CHECK(reloaded.quickStartDone());
}
