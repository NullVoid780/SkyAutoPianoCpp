#pragma once

#include <functional>
#include <map>
#include <optional>
#include <vector>

#include <QColor>
#include <QString>

#include "ui/theme.hpp"

namespace sky {

enum class PreviewScene { Main, Popups, Alerts, Settings, Editor };

struct ThemeSlot {
    QString id;
    QString label;
    std::function<QColor(const Theme &)> read;
    std::function<void(Theme &, const QColor &)> write;
    bool core = false;
};

struct ThemeSection {
    QString id;
    QString title;
    QString description;
    QString iconId;
    std::vector<ThemeSlot> entries;
    PreviewScene scene = PreviewScene::Main;
};

using ThemeColors = std::map<QString, QColor>;

struct QuickColor {
    QString id;
    QString label;
    QString description;
    QString primary;
    std::vector<std::pair<QString, std::function<QColor(const QColor &)>>> targets;
};

struct ThemeDefinition {
    QString id;
    QString name;
    ThemeColors light;
    ThemeColors dark;
    bool builtIn = false;

    ThemeColors &colors(AppTheme kind) { return kind == AppTheme::Dark ? dark : light; }
    const ThemeColors &colors(AppTheme kind) const { return kind == AppTheme::Dark ? dark : light; }
};

inline constexpr const char *kDefaultThemeId = "builtin-default";

const std::vector<ThemeSection> &themeSections();
const ThemeSlot *findThemeSlot(const QString &id);

struct QuickColorSet {
    PreviewScene scene = PreviewScene::Main;
    QString name;
    QString description;
    std::vector<QuickColor> colors;
};

const std::vector<QuickColorSet> &quickColorSets();
const QuickColorSet &quickColorSet(PreviewScene scene);
const std::vector<QuickColor> &themeQuickColors();
void applyQuickColor(ThemeColors &colors, const QuickColor &quick, const QColor &color);
bool isSlotChanged(const ThemeColors &colors, const ThemeColors &start, const QString &slotId);
void restoreSlot(ThemeColors &colors, const ThemeColors &start, const QString &slotId);
bool isQuickColorChanged(const ThemeColors &colors, const ThemeColors &start, const QuickColor &quick);
void restoreQuickColor(ThemeColors &colors, const ThemeColors &start, const QuickColor &quick);

const std::vector<ThemeDefinition> &builtInThemes();
const ThemeDefinition *findBuiltInTheme(const QString &id);
QString defaultThemeId();

Theme composeTheme(AppTheme base, const ThemeColors &colors);
Theme composeTheme(const ThemeDefinition &definition, AppTheme kind);

QString colorToText(const QColor &color);
std::optional<QColor> colorFromText(const QString &text);

}  // namespace sky
