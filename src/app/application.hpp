#pragma once

#include <map>
#include <memory>
#include <string>

#include <QObject>
#include <QPointer>

#include <optional>

#include "model/config.hpp"
#include "services/library/legacy_import.hpp"
#include "services/library/library_service.hpp"
#include "services/notifications/toast_service.hpp"

namespace sky {

class ChangelogPanel;
class IssueRecorder;
class SkyWindowWatcher;
class IssueReportWindow;
class RecordingBar;
class ConfigService;
class EditorWindow;
class FavoritesService;
class LibraryService;
class MainWindow;
class PlaybackService;
class PlayerController;
class PlaylistService;
class QuickStart;
class RecentService;
class SettingsWindow;
class ShortcutService;
class StoreService;
class ThemeController;
class ThemeLibraryService;
class ToastService;
class TrayNotification;
class UpdateController;
class UiStateService;
class UpdateService;

class Application final : public QObject {
    Q_OBJECT

public:
    Application();
    ~Application() override;

    void start(bool afterCrash = false);
    void activate();
    bool relaunchRequested() const { return m_relaunch; }
    bool launchPendingInstaller();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void wire();
    void toggleTheme();
    void applyTheme(AppTheme theme);
    void openSettings();
    void openEditor(int libraryIndex);
    void updateCrashState();
    void updateSkyStatus();
    void showPendingChangelog();
    void presentReleaseNotes(const std::string &version, const std::string &knownNotes);
    void requestRelaunch();
    void detectLegacyLibrary(bool manual);
    void onLegacyDetected(std::optional<LegacyImportPlan> plan, bool manual);
    void pickLegacyFolder();
    void importLegacy(const LegacyImportPlan &plan, bool manual);
    void notifyLegacy(bool manual, const Toast &toast);
    void showQuickStart();
    void startIssueRecording();
    void finishIssueRecording();
    void saveCrashReport(const QString &id);
    void refreshCrashList();
    OrderedJson diagnostics() const;
    void applyStartupLayout();
    void tell(const Toast &toast);
    // Per-category outcome of applying the non-sheet legacy extras, so the
    // import can build an honest LegacyImportResult instead of a bare count.
    struct LegacyExtrasOutcome {
        std::string text;
        int playlistsCreated = 0;
        int playlistsMerged = 0;
        int playlistReferencesRemapped = 0;
        int playlistMissingReferences = 0;
        int favoritesImported = 0;
        int favoritesAlreadyPresent = 0;
        bool recentImported = false;
        bool recentSkipped = false;
        bool settingsApplied = false;
        bool settingsRejected = false;
        bool themeApplied = false;
        bool themeSkipped = false;
    };
    LegacyExtrasOutcome applyLegacyExtras(const LegacyImportPlan &plan, const LegacyImportSummary &summary);
    std::map<std::string, std::string> legacyExistingPayloads() const;

    std::unique_ptr<ConfigService> m_config;
    std::unique_ptr<UiStateService> m_uiState;
    std::unique_ptr<FavoritesService> m_favorites;
    std::unique_ptr<RecentService> m_recent;
    std::unique_ptr<PlaylistService> m_playlists;
    std::unique_ptr<LibraryService> m_library;
    std::unique_ptr<ToastService> m_toasts;
    std::unique_ptr<PlaybackService> m_playback;
    std::unique_ptr<ShortcutService> m_shortcuts;
    std::unique_ptr<StoreService> m_store;
    std::unique_ptr<UpdateService> m_updates;
    std::unique_ptr<ThemeLibraryService> m_themeLibrary;
    std::unique_ptr<ThemeController> m_themes;
    std::unique_ptr<PlayerController> m_player;
    std::unique_ptr<MainWindow> m_mainWindow;
    QPointer<SettingsWindow> m_settings;
    QPointer<EditorWindow> m_editor;
    TrayNotification *m_tray = nullptr;
    UpdateController *m_updateController = nullptr;
    ChangelogPanel *m_changelog = nullptr;
    bool m_relaunch = false;
    bool m_legacyBusy = false;
    QPointer<QuickStart> m_quickStart;
    IssueRecorder *m_recorder = nullptr;
    SkyWindowWatcher *m_skyWatcher = nullptr;
    bool m_skyHeld = false;
    QPointer<RecordingBar> m_recordingBar;
    QPointer<IssueReportWindow> m_reportWindow;
    std::optional<LegacyImportPlan> m_quickStartLegacy;
    bool m_legacyForQuickStart = false;
};

}  // namespace sky
