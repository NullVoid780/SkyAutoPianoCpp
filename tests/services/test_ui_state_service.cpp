#include <catch2/catch_test_macros.hpp>

#include <fstream>

#include "core/text_file.hpp"
#include "qt_test_support.hpp"
#include "services/library/favorites_service.hpp"
#include "services/library/recent_service.hpp"
#include "services/settings/ui_state_service.hpp"

using namespace sky;

TEST_CASE("ui state defaults to the light theme")
{
    ensureCoreApp();
    TempDir dir;
    UiStateService state(dir.path());
    state.load();
    CHECK(state.theme() == AppTheme::Light);
    CHECK(state.favorites().empty());
}

TEST_CASE("theme, favourites and recent plays persist to ui-state.json")
{
    ensureCoreApp();
    TempDir dir;
    {
        UiStateService state(dir.path());
        state.load();
        FavoritesService favorites(state);
        RecentService recent(state);
        state.setTheme(AppTheme::Dark);
        favorites.toggle("A");
        favorites.toggle("B");
        favorites.toggle("A");
        for (int i = 0; i < 12; ++i) {
            recent.add("S" + std::to_string(i));
        }
        recent.add("S5");
        CHECK(recent.names().size() == kRecentPlaysLimit);
        CHECK(recent.names().front() == "S5");
        CHECK(recent.names()[1] == "S11");
    }
    UiStateService reloaded(dir.path());
    reloaded.load();
    FavoritesService favorites(reloaded);
    CHECK(reloaded.theme() == AppTheme::Dark);
    CHECK(favorites.isFavorite("B"));
    CHECK_FALSE(favorites.isFavorite("A"));
    CHECK(reloaded.recentPlays().front() == "S5");
    const std::string text = readTextFile(dir.path() / "ui-state.json").value();
    CHECK(text.rfind("{\n    \"theme\": \"dark\",", 0) == 0);
}

TEST_CASE("unknown ui-state fields survive a save")
{
    ensureCoreApp();
    TempDir dir;
    {
        std::ofstream out(dir.path() / "ui-state.json", std::ios::binary);
        out << R"({"theme":"dark","future":42,"favorites":["x",3]})";
    }
    UiStateService state(dir.path());
    state.load();
    CHECK(state.favorites() == std::vector<std::string>{"x"});
    state.setRecentPlays({"y"});
    const std::string text = readTextFile(dir.path() / "ui-state.json").value();
    CHECK(text.find("\"future\": 42") != std::string::npos);
}
