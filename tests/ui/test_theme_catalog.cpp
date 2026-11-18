#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QSet>

#include "ui/theme_catalog.hpp"
#include "ui/settings_window/theme_preview.hpp"
#include "ui/theme_manager.hpp"

using namespace sky;

TEST_CASE("theme slot ids are unique and every slot reads back what it writes")
{
    QSet<QString> seen;
    const QColor probe(18, 52, 86, 200);
    for (const ThemeSection &section : themeSections()) {
        CHECK_FALSE(section.entries.empty());
        for (const ThemeSlot &entry : section.entries) {
            INFO(entry.id.toStdString());
            CHECK_FALSE(seen.contains(entry.id));
            seen.insert(entry.id);
            Theme theme = Theme::dark();
            entry.write(theme, probe);
            CHECK(entry.read(theme) == probe);
        }
    }
    CHECK(seen.size() > 60);
}

TEST_CASE("a theme with no colors matches the built-in look")
{
    const Theme light = composeTheme(AppTheme::Light, {});
    const Theme dark = composeTheme(AppTheme::Dark, {});
    CHECK(light.qssTokens() == Theme::light().qssTokens());
    CHECK(dark.qssTokens() == Theme::dark().qssTokens());
    CHECK(dark.cardFill == Theme::dark().background);
    CHECK(light.footerText == Theme::light().fontColor);
    CHECK(dark.progressPlayed == Theme::dark().progressGroove);
}

TEST_CASE("core colors flow into the parts built from them")
{
    const QColor text("#ffeedd");
    const Theme theme = composeTheme(AppTheme::Dark, {{QStringLiteral("app.text"), text}});
    CHECK(theme.fontColor == text);
    CHECK(theme.cardText == text);
    CHECK(theme.footerText == text);
    CHECK(theme.progressGroove == text);
    CHECK(theme.borderColor == text);
}

TEST_CASE("section colors override only their own part")
{
    const QColor fill("#203040");
    const Theme theme = composeTheme(AppTheme::Light, {{QStringLiteral("card.fill"), fill},
                                                       {QStringLiteral("footer.play"), QColor("#00aa00")}});
    CHECK(theme.cardFill == fill);
    CHECK(theme.footerFill == Theme::light().background);
    CHECK(theme.transportPlay == QColor("#00aa00"));
    CHECK(theme.transportPlayHover != theme.transportPlay);
}

TEST_CASE("built-in themes have both modes, use known slots and render a full stylesheet")
{
    QFile file(QStringLiteral(SKY_QSS_PATH));
    REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(file.readAll());
    const ThemeDefinition *fallback = findBuiltInTheme(defaultThemeId());
    REQUIRE(fallback);
    CHECK(composeTheme(*fallback, AppTheme::Dark).qssTokens() == Theme::dark().qssTokens());
    QSet<QString> ids;
    for (const ThemeDefinition &definition : builtInThemes()) {
        INFO(definition.id.toStdString());
        CHECK(definition.builtIn);
        CHECK_FALSE(ids.contains(definition.id));
        ids.insert(definition.id);
        if (definition.id != defaultThemeId()) {
            CHECK_FALSE(definition.light.empty());
            CHECK_FALSE(definition.dark.empty());
        }
        for (AppTheme kind : {AppTheme::Light, AppTheme::Dark}) {
            for (const auto &[slotId, color] : definition.colors(kind)) {
                INFO(slotId.toStdString());
                CHECK(findThemeSlot(slotId));
                CHECK(color.isValid());
            }
            const Theme theme = composeTheme(definition, kind);
            CHECK(theme.kind == kind);
            CHECK_FALSE(ThemeManager::renderStyleSheet(text, theme).contains(QLatin1Char('@')));
        }
    }
}

TEST_CASE("color text round-trips with and without opacity")
{
    CHECK(colorToText(QColor("#AABBCC")) == QStringLiteral("#aabbcc"));
    CHECK(colorToText(QColor(1, 2, 3, 128)) == QStringLiteral("#01020380"));
    REQUIRE(colorFromText(QStringLiteral("#01020380")));
    CHECK(*colorFromText(QStringLiteral("#01020380")) == QColor(1, 2, 3, 128));
    CHECK(*colorFromText(QStringLiteral(" aabbcc ")) == QColor("#aabbcc"));
    CHECK_FALSE(colorFromText(QStringLiteral("#abc")));
    CHECK_FALSE(colorFromText(QStringLiteral("blue")));
}

TEST_CASE("quick colors fill every part they cover and reset cleanly")
{
    REQUIRE(quickColorSets().size() == 5);
    QSet<int> scenes;
    QSet<QString> ids;
    std::vector<const QuickColor *> all;
    for (const QuickColorSet &set : quickColorSets()) {
        INFO(set.name.toStdString());
        CHECK_FALSE(scenes.contains(static_cast<int>(set.scene)));
        scenes.insert(static_cast<int>(set.scene));
        CHECK(&quickColorSet(set.scene) == &set);
        CHECK(set.colors.size() == 5);
        for (const QuickColor &quick : set.colors) {
            all.push_back(&quick);
        }
    }
    CHECK(&themeQuickColors() == &quickColorSet(PreviewScene::Main).colors);
    for (const QuickColor *entry : all) {
        const QuickColor &quick = *entry;
        INFO(quick.id.toStdString());
        CHECK_FALSE(ids.contains(quick.id));
        ids.insert(quick.id);
        CHECK(findThemeSlot(quick.primary));
        bool hasPrimary = false;
        for (const auto &target : quick.targets) {
            CHECK(findThemeSlot(target.first));
            hasPrimary = hasPrimary || target.first == quick.primary;
        }
        CHECK(hasPrimary);
        ThemeColors colors;
        const ThemeColors start;
        applyQuickColor(colors, quick, QColor("#336699"));
        CHECK(isQuickColorChanged(colors, start, quick));
        CHECK(colors.size() == quick.targets.size());
        CHECK(colors[quick.primary] == QColor("#336699"));
        restoreQuickColor(colors, start, quick);
        CHECK(colors.empty());
        CHECK_FALSE(isQuickColorChanged(colors, start, quick));
    }
    ThemeColors accent;
    applyQuickColor(accent, themeQuickColors().front(), QColor("#ff0000"));
    const Theme theme = composeTheme(AppTheme::Light, accent);
    CHECK(theme.hoverColor == QColor("#ff0000"));
    CHECK(theme.progressPlayed == QColor("#ff0000"));
    CHECK(theme.navActiveEnd != QColor("#ff0000"));
}

TEST_CASE("changes are measured against the theme the editor started from")
{
    const ThemeDefinition *indigo = findBuiltInTheme(QStringLiteral("builtin-midnight"));
    REQUIRE(indigo);
    const ThemeColors start = indigo->light;
    ThemeColors colors = start;
    for (const ThemeSection &section : themeSections()) {
        for (const ThemeSlot &entry : section.entries) {
            CHECK_FALSE(isSlotChanged(colors, start, entry.id));
        }
    }
    for (const QuickColor &quick : themeQuickColors()) {
        CHECK_FALSE(isQuickColorChanged(colors, start, quick));
    }

    colors[QStringLiteral("card.fill")] = QColor("#123456");
    colors[QStringLiteral("toast.info")] = QColor("#654321");
    colors.erase(QStringLiteral("footer.fill"));
    CHECK(isSlotChanged(colors, start, QStringLiteral("card.fill")));
    CHECK(isSlotChanged(colors, start, QStringLiteral("toast.info")));
    CHECK(isSlotChanged(colors, start, QStringLiteral("footer.fill")));
    CHECK_FALSE(isSlotChanged(colors, start, QStringLiteral("app.text")));

    restoreSlot(colors, start, QStringLiteral("card.fill"));
    restoreSlot(colors, start, QStringLiteral("toast.info"));
    restoreSlot(colors, start, QStringLiteral("footer.fill"));
    CHECK(colors == start);
}

TEST_CASE("built-in themes have plain names")
{
    QStringList names;
    for (const ThemeDefinition &definition : builtInThemes()) {
        names << definition.name;
    }
    CHECK(names == QStringList{QStringLiteral("Default"), QStringLiteral("Indigo"), QStringLiteral("Green"),
                               QStringLiteral("Orange"), QStringLiteral("Pink"), QStringLiteral("Blue"),
                               QStringLiteral("Black and White")});
}

TEST_CASE("the title bar follows the window background unless it is set")
{
    CHECK(Theme::dark().titleBarFill == Theme::dark().windowMain);
    CHECK(Theme::light().titleBarText == Theme::light().fontColor);
    const Theme following = composeTheme(AppTheme::Dark, {{QStringLiteral("main.window"), QColor("#102030")},
                                                          {QStringLiteral("app.text"), QColor("#eeddcc")}});
    CHECK(following.titleBarFill == QColor("#102030"));
    CHECK(following.windowBorder == QColor("#102030"));
    CHECK(following.titleBarText == QColor("#eeddcc"));
    const Theme custom = composeTheme(AppTheme::Dark, {{QStringLiteral("main.window"), QColor("#102030")},
                                                       {QStringLiteral("title.fill"), QColor("#ff0000")}});
    CHECK(custom.titleBarFill == QColor("#ff0000"));
    CHECK(custom.windowBorder == QColor("#ff0000"));
}

TEST_CASE("every color can be highlighted on the preview screen of its section")
{
    const QStringList notDrawn = {QStringLiteral("app.success"), QStringLiteral("app.error"),
                                  QStringLiteral("card.selected"), QStringLiteral("card.storeTag"),
                                  QStringLiteral("drawer.selectionBar")};
    for (const ThemeSection &section : themeSections()) {
        const QStringList shown = previewSlotIds(section.scene);
        for (const ThemeSlot &entry : section.entries) {
            INFO(entry.id.toStdString());
            if (!notDrawn.contains(entry.id)) {
                CHECK(shown.contains(entry.id));
            }
        }
    }
}
