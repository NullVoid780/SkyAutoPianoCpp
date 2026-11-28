#include <catch2/catch_test_macros.hpp>

#include <fstream>

#include "qt_test_support.hpp"
#include "services/settings/config_service.hpp"
#include "services/settings/theme_library_service.hpp"
#include "services/settings/ui_state_service.hpp"
#include "ui/controllers/theme_controller.hpp"
#include "ui/theme_manager.hpp"

using namespace sky;

namespace {

struct ThemeFixture {
    bool appReady = (ensureCoreApp(), true);
    TempDir dir;
    ConfigService config{dir.path() / "config"};
    UiStateService uiState{dir.path() / "data"};
    ThemeLibraryService themeLibrary{dir.path() / "data"};
    ThemeController controller{uiState, config, themeLibrary};

    ThemeFixture() {
        config.load();
        uiState.load();
        themeLibrary.load();
        controller.setFollowSystem(false);
    }
};

}  // namespace

TEST_CASE("ThemeController selects, toggles, and persists built-in and custom themes") {
    ThemeFixture f;
    f.controller.applyStartup();
    CHECK(f.controller.kind() == AppTheme::Light);

    f.controller.toggle();
    CHECK(f.controller.kind() == AppTheme::Dark);
    CHECK(f.uiState.theme() == AppTheme::Dark);
    CHECK(ThemeManager::instance().kind() == AppTheme::Dark);

    ThemeDefinition def;
    def.name = QStringLiteral("Ocean");
    def.light[QStringLiteral("background")] = QColor(240, 248, 255);
    const Result<QString> saved = f.controller.save(def);
    REQUIRE(saved.ok());
    CHECK(f.controller.customThemes().size() == 1);

    f.controller.select(saved.value());
    CHECK(f.controller.activeId() == saved.value());
    CHECK(f.uiState.themeId() == saved.value().toStdString());

    // Preview temporarily changes ThemeManager without overwriting persisted selection.
    ThemeDefinition draft = def;
    draft.name = QStringLiteral("Preview");
    f.controller.preview(draft, AppTheme::Light);
    CHECK(f.controller.isPreviewing());
    CHECK(ThemeManager::instance().kind() == AppTheme::Light);
    f.controller.endPreview();
    CHECK_FALSE(f.controller.isPreviewing());
    CHECK(ThemeManager::instance().kind() == AppTheme::Dark);

    // Export and re-import.
    const auto exported = f.dir.path() / "ocean.skytheme.json";
    REQUIRE(f.controller.exportFile(saved.value(), exported).ok());
    REQUIRE(std::filesystem::exists(exported));
    const Result<QString> imported = f.controller.importFile(exported);
    REQUIRE(imported.ok());
    CHECK(f.controller.customThemes().size() == 2);

    // Removing the active custom theme falls back to default built-in theme.
    CHECK(f.controller.remove(saved.value()));
    CHECK(f.controller.activeId() == QString::fromLatin1(kDefaultThemeId));
}

TEST_CASE("ThemeController rejects invalid theme imports and exports") {
    ThemeFixture f;
    const auto badFile = f.dir.path() / "broken.json";
    {
        std::ofstream out(badFile, std::ios::binary);
        out << "{not json";
    }
    CHECK_FALSE(f.controller.importFile(badFile).ok());
    CHECK_FALSE(
        f.controller.exportFile(QStringLiteral("missing-id"), f.dir.path() / "out.json").ok());
    CHECK_FALSE(f.controller.remove(QStringLiteral("missing-id")));
}
