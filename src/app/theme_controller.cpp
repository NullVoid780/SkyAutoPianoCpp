#include "app/theme_controller.hpp"

#include <QGuiApplication>
#include <QStyleHints>

#include "core/translator.hpp"
#include "core/string_utils.hpp"
#include "services/settings/config_service.hpp"
#include "services/settings/theme_library_service.hpp"
#include "services/settings/ui_state_service.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

ThemeController::ThemeController(UiStateService &uiState, ConfigService &config, ThemeLibraryService &library,
                                 QObject *parent)
    : QObject(parent), m_uiState(uiState), m_config(config), m_library(library)
{
    connect(&m_library, &ThemeLibraryService::changed, this, &ThemeController::themesChanged);
    if (qGuiApp) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &ThemeController::syncWithSystem);
    }
}

std::optional<AppTheme> ThemeController::systemKind()
{
    if (!qGuiApp) {
        return std::nullopt;
    }
    switch (QGuiApplication::styleHints()->colorScheme()) {
    case Qt::ColorScheme::Dark:
        return AppTheme::Dark;
    case Qt::ColorScheme::Light:
        return AppTheme::Light;
    default:
        return std::nullopt;
    }
}

bool ThemeController::followsSystem() const
{
    return m_uiState.followSystemTheme();
}

void ThemeController::setFollowSystem(bool follow)
{
    if (follow == followsSystem()) {
        return;
    }
    m_uiState.setFollowSystemTheme(follow);
    emit followSystemChanged(follow);
    if (follow) {
        syncWithSystem();
    }
}

void ThemeController::storeKind(AppTheme kind)
{
    m_uiState.setTheme(kind);
    m_config.setTheme(kind);
}

void ThemeController::syncWithSystem()
{
    if (!followsSystem()) {
        return;
    }
    const auto system = systemKind();
    if (!system || *system == kind()) {
        return;
    }
    storeKind(*system);
    if (m_previewing) {
        emit activeChanged(activeId());
    } else {
        applyActive();
    }
}

ThemeDefinition ThemeController::fromCustom(const CustomTheme &theme)
{
    ThemeDefinition definition;
    definition.id = toQString(theme.id);
    definition.name = toQString(theme.name);
    for (AppTheme kind : {AppTheme::Light, AppTheme::Dark}) {
        for (const auto &[key, value] : kind == AppTheme::Dark ? theme.dark : theme.light) {
            if (const auto color = colorFromText(toQString(value))) {
                definition.colors(kind)[toQString(key)] = *color;
            }
        }
    }
    return definition;
}

CustomTheme ThemeController::toCustom(const ThemeDefinition &definition)
{
    CustomTheme theme;
    theme.id = definition.builtIn ? std::string() : fromQString(definition.id);
    theme.name = fromQString(definition.name);
    for (AppTheme kind : {AppTheme::Light, AppTheme::Dark}) {
        ThemeColorList &target = kind == AppTheme::Dark ? theme.dark : theme.light;
        for (const auto &[key, color] : definition.colors(kind)) {
            if (findThemeSlot(key)) {
                target.emplace_back(fromQString(key), fromQString(colorToText(color)));
            }
        }
    }
    return theme;
}

std::vector<ThemeDefinition> ThemeController::customThemes() const
{
    std::vector<ThemeDefinition> themes;
    for (const CustomTheme &theme : m_library.themes()) {
        themes.push_back(fromCustom(theme));
    }
    return themes;
}

std::optional<ThemeDefinition> ThemeController::definition(const QString &id) const
{
    if (const ThemeDefinition *builtIn = findBuiltInTheme(id)) {
        return *builtIn;
    }
    if (const CustomTheme *custom = m_library.find(fromQString(id))) {
        return fromCustom(*custom);
    }
    return std::nullopt;
}

QString ThemeController::activeId() const
{
    const QString id = toQString(m_uiState.themeId());
    return definition(id) ? id : defaultThemeId();
}

AppTheme ThemeController::kind() const
{
    return m_uiState.theme();
}

void ThemeController::applyActive()
{
    m_previewing = false;
    const QString id = activeId();
    const auto active = definition(id);
    ThemeManager::instance().apply(active ? composeTheme(*active, kind()) : Theme::forKind(kind()));
    emit activeChanged(id);
}

void ThemeController::applyStartup()
{
    if (followsSystem()) {
        if (const auto system = systemKind()) {
            storeKind(*system);
        }
    }
    applyActive();
}

void ThemeController::select(const QString &id)
{
    if (!definition(id)) {
        return;
    }
    m_uiState.setThemeId(id == defaultThemeId() ? std::string() : fromQString(id));
    applyActive();
}

void ThemeController::toggle()
{
    setKind(m_uiState.theme() == AppTheme::Dark ? AppTheme::Light : AppTheme::Dark);
}

void ThemeController::setKind(AppTheme kind)
{
    setFollowSystem(false);
    storeKind(kind);
    applyActive();
}

void ThemeController::preview(const ThemeDefinition &draft, AppTheme kind)
{
    m_previewing = true;
    ThemeManager::instance().apply(composeTheme(draft, kind));
}

void ThemeController::endPreview()
{
    if (m_previewing) {
        applyActive();
    }
}

Result<QString> ThemeController::save(const ThemeDefinition &definition)
{
    auto saved = m_library.save(toCustom(definition));
    if (!saved) {
        return saved.error();
    }
    const QString id = toQString(saved.value());
    if (!m_previewing && id == activeId()) {
        applyActive();
    }
    return id;
}

bool ThemeController::remove(const QString &id)
{
    const bool wasActive = id == activeId();
    if (!m_library.remove(fromQString(id))) {
        return false;
    }
    if (toQString(m_uiState.themeId()) == id) {
        m_uiState.setThemeId(std::string());
    }
    if (wasActive && !m_previewing) {
        applyActive();
    }
    return true;
}

Result<QString> ThemeController::importFile(const std::filesystem::path &file)
{
    auto read = ThemeLibraryService::readThemeFile(file);
    if (!read) {
        return read.error();
    }
    auto saved = m_library.save(std::move(read).value());
    if (!saved) {
        return saved.error();
    }
    return toQString(saved.value());
}

Result<void> ThemeController::exportFile(const QString &id, const std::filesystem::path &file) const
{
    const auto found = definition(id);
    if (!found) {
        return Error{"THEME_EXPORT_FAILED", trUtf8("The theme could not be exported."), {}};
    }
    return ThemeLibraryService::writeThemeFile(file, toCustom(*found));
}

QString ThemeController::suggestName(const QString &wanted, const QString &ignoreId) const
{
    return toQString(m_library.uniqueName(fromQString(wanted), fromQString(ignoreId)));
}

}  // namespace sky
