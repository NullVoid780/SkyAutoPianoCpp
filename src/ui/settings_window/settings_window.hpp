#pragma once

#include <vector>

#include <QWidget>

#include "core/json.hpp"
#include "ui/settings_window/reports_tab.hpp"
#include "model/config.hpp"
#include "services/notifications/toast_service.hpp"
#include "services/update/update_service.hpp"
#include "ui/settings_window/update_overview_card.hpp"

class QScrollArea;
class QVariantAnimation;
class QVBoxLayout;

namespace sky {

class ChangelogPanel;
class ConfirmDialog;
class ConfigService;
class ElevationLayer;
class GeneralTab;
class KeyboardTab;
class SaveButton;
class SettingsSidebar;
class SmoothScroll;
class ShortcutsTab;
class ThemeController;
class ThemesTab;
class ToastHost;
class UpdateService;
class UpdatesTab;
class AboutTab;

struct SettingsServices {
    ConfigService &config;
    UpdateService &updates;
    ThemeController &themes;
};

class SettingsWindow final : public QWidget {
    Q_OBJECT

public:
    SettingsWindow(SettingsServices services, QWidget *owner);

    void showToast(const Toast &toast);
    void reloadForm();
    void setLanguage(const QString &preference);
    int currentPage() const;
    void openPage(int index);
    void setCrashReports(const std::vector<CrashListEntry> &crashes);
    void setLegacyCleanup(const QString &description, bool removeEnabled, bool busy, bool present);
    bool isLegacyCleanupShown() const;

signals:
    void themeImported(sky::AppTheme theme);
    void importPreviousRequested();
    void legacyRemovalRequested();
    void legacyCleanupShown();
    void quickStartRequested();
    void languageChosen(const QString &preference);
    void restartRequested();
    void recordIssueRequested();
    void saveCrashReportRequested(const QString &id);
    void crashFolderRequested();
    void clearCrashReportsRequested();
    void updateAvailable(const sky::VersionCheck &check);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void showPage(int index);
    void loadForm(const Config &config);
    Config formConfig() const;
    OrderedJson mergedJson(const Config &form) const;
    bool save();
    bool hasUnsavedChanges() const;
    void exportSettings();
    void importSettings();
    void applyImported(Result<OrderedJson> imported);
    void checkForUpdates();
    void showWhatsNew();
    void showUpdateResult(UpdateOverviewCard::State state, const QString &latestVersion = QString());
    void onBlockUpdatesToggled(bool blocked);
    void notify(ToastType type, const std::string &message);
    void positionOverlays();
    void applyPageMargins(int rise);

    SettingsServices m_services;
    SettingsSidebar *m_sidebar;
    QScrollArea *m_scroll;
    SmoothScroll *m_smoothScroll = nullptr;
    QWidget *m_pageHost;
    QVBoxLayout *m_pageLayout;
    GeneralTab *m_general;
    KeyboardTab *m_keyboard;
    ShortcutsTab *m_shortcuts;
    ThemesTab *m_themes;
    UpdatesTab *m_updates;
    ReportsTab *m_reports;
    AboutTab *m_about;
    int m_updatesNav = -1;
    int m_generalNav = -1;
    std::vector<QWidget *> m_pages;
    SaveButton *m_save;
    ElevationLayer *m_shadows;
    ConfirmDialog *m_blockDialog;
    ConfirmDialog *m_unsavedDialog;
    ConfirmDialog *m_clearCrashesDialog;
    ChangelogPanel *m_whatsNew;
    ToastHost *m_toasts;
    QVariantAnimation *m_pageFade;
    bool m_busy = false;
    bool m_closing = false;
    bool m_loadingWhatsNew = false;
};

}  // namespace sky
