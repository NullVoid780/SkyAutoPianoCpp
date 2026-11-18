#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QObject>
#include <QString>

#include "core/result.hpp"
#include "model/config.hpp"
#include "ui/theme_catalog.hpp"

namespace sky {

class ConfigService;
class ThemeLibraryService;
class UiStateService;
struct CustomTheme;

class ThemeController final : public QObject {
    Q_OBJECT

public:
    ThemeController(UiStateService &uiState, ConfigService &config, ThemeLibraryService &library,
                    QObject *parent = nullptr);

    void applyStartup();

    std::vector<ThemeDefinition> customThemes() const;
    std::optional<ThemeDefinition> definition(const QString &id) const;
    QString activeId() const;
    AppTheme kind() const;
    bool isPreviewing() const { return m_previewing; }

    void select(const QString &id);
    void toggle();
    void setKind(AppTheme kind);
    bool followsSystem() const;
    void setFollowSystem(bool follow);
    static std::optional<AppTheme> systemKind();

    void preview(const ThemeDefinition &draft, AppTheme kind);
    void endPreview();

    Result<QString> save(const ThemeDefinition &definition);
    bool remove(const QString &id);
    Result<QString> importFile(const std::filesystem::path &file);
    Result<void> exportFile(const QString &id, const std::filesystem::path &file) const;
    QString suggestName(const QString &wanted, const QString &ignoreId = {}) const;

    static ThemeDefinition fromCustom(const CustomTheme &theme);
    static CustomTheme toCustom(const ThemeDefinition &definition);

signals:
    void activeChanged(const QString &id);
    void themesChanged();
    void followSystemChanged(bool follow);

private:
    void applyActive();
    void storeKind(AppTheme kind);
    void syncWithSystem();

    UiStateService &m_uiState;
    ConfigService &m_config;
    ThemeLibraryService &m_library;
    bool m_previewing = false;
};

}  // namespace sky
