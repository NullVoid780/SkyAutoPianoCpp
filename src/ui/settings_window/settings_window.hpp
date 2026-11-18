#pragma once

#include <vector>

#include <QWidget>

#include "core/json.hpp"
#include "model/config.hpp"
#include "services/notifications/toast_service.hpp"
#include "services/update/update_service.hpp"
#include "ui/settings_window/update_overview_card.hpp"

class QScrollArea;
class QVariantAnimation;
class QVBoxLayout;

namespace sky {

class BlockUpdatesDialog;
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

signals:
    void themeImported(sky::AppTheme theme);
    void importPreviousRequested();
    void quickStartRequested();
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
    void save();
    void exportSettings();
    void importSettings();
    void applyImported(Result<OrderedJson> imported);
    void checkForUpdates();
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
    AboutTab *m_about;
    int m_updatesNav = -1;
    std::vector<QWidget *> m_pages;
    SaveButton *m_save;
    ElevationLayer *m_shadows;
    BlockUpdatesDialog *m_blockDialog;
    ToastHost *m_toasts;
    QVariantAnimation *m_pageFade;
    bool m_busy = false;
};

}  // namespace sky
