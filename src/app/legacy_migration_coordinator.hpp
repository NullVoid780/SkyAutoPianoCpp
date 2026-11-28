#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QThreadPool>

#include "services/library/legacy_import.hpp"
#include "services/library/legacy_removal.hpp"
#include "services/notifications/toast_service.hpp"

class QTimer;

namespace sky {

class ConfigService;
class FavoritesService;
class LibraryService;
class PlaylistService;
class RecentService;
class ShortcutService;
class ThemeController;
class UiStateService;
struct LegacyImportSummary;

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

enum class LegacyQuickStartState { Searching, Found, NotFound, UpToDate, Importing, Imported };

struct LegacyCleanupPresentation {
    QString title;
    QString description;
    bool removeEnabled = false;
    bool busy = false;
    bool present = false;
};

struct LegacyCoordinatorEnvironment {
    std::filesystem::path appDirectory;
    std::filesystem::path dataDirectory;
    std::filesystem::path configDirectory;
    std::filesystem::path localStorageDirectory;
    std::vector<std::filesystem::path> searchRoots;
    bool useRegistryHints = true;
    std::shared_ptr<ILegacyRemovalSystem> removalSystem;
    std::function<bool()> libraryIntactCheck;

    static LegacyCoordinatorEnvironment production();
};

LegacyImportResult aggregateLegacyImportResult(const LegacyImportPlan &plan,
                                               const LegacyImportSummary &summary,
                                               const LegacyExtrasOutcome &outcome,
                                               const std::string &fingerprint,
                                               const std::filesystem::path &destDataDirectory,
                                               bool statePersisted = true);

LegacyImportEvidence
buildLegacyRemovalEvidence(const UiStateService &uiState, const LegacyInstallInfo &install,
                           const std::optional<LegacyImportEvidence> &sessionEvidence);

class LegacyMigrationCoordinator final : public QObject {
    Q_OBJECT

public:
    LegacyMigrationCoordinator(
        LibraryService &library, PlaylistService &playlists, FavoritesService &favorites,
        RecentService &recent, ConfigService &config, ShortcutService &shortcuts,
        ThemeController &themes, UiStateService &uiState,
        LegacyCoordinatorEnvironment env = LegacyCoordinatorEnvironment::production(),
        QObject *parent = nullptr);
    ~LegacyMigrationCoordinator() override;

    void shutdown();

    void detect(bool manual, bool forQuickStart);
    void inspectFolder(const std::filesystem::path &folder, bool forQuickStart);
    void cancelQuickStartFolderPick();
    void importPlan(const LegacyImportPlan &plan, bool manual);
    bool importPendingQuickStartPlan();

    void refreshCleanup();
    void setCleanupVisibleProvider(std::function<bool()> provider);
    void armAutoVerify();
    void startVerification();
    RemovalGate checkRemovalGate();
    void requestRemoval();
    void pollRemoval();

    bool isBusy() const { return m_busy; }
    bool isVerifying() const { return m_verifying; }
    const std::optional<LegacyImportPlan> &pendingQuickStartPlan() const {
        return m_quickStartPlan;
    }
    const std::optional<LegacyDetection> &removalDetection() const { return m_removalDetection; }
    const std::optional<LegacyImportEvidence> &removalEvidence() const { return m_removalEvidence; }
    QString legacyOldVersion() const;
    QString legacyCleanupTitle() const;

signals:
    void toastRequested(const sky::Toast &toast, bool preferSettings);
    void autoImportPromptRequested(const sky::LegacyImportPlan &plan, const QString &prompt);
    void folderPickRequested();
    void quickStartStateChanged(sky::LegacyQuickStartState state, int songs, int playlists,
                                const QString &detail);
    void cleanupPresentationChanged(const sky::LegacyCleanupPresentation &presentation);
    void importCompleted(const sky::LegacyImportResult &result, bool manual,
                         const QString &summaryText);
    void verificationFinished(bool verified, int failed);
    void removalOutcomeObserved(const sky::RemovalOutcome &outcome);

private:
    std::map<std::string, std::string> existingPayloads() const;
    void onDetected(std::optional<LegacyImportPlan> plan, bool manual, bool forQuickStart);
    LegacyExtrasOutcome applyExtras(const LegacyImportPlan &plan,
                                    const LegacyImportSummary &summary);
    void emitCleanupPresentation();
    void maybeAutoVerify();

    LibraryService &m_library;
    PlaylistService &m_playlists;
    FavoritesService &m_favorites;
    RecentService &m_recent;
    ConfigService &m_config;
    ShortcutService &m_shortcuts;
    ThemeController &m_themes;
    UiStateService &m_uiState;
    LegacyCoordinatorEnvironment m_env;

    std::unique_ptr<LegacyRemovalService> m_removal;
    std::unique_ptr<QThreadPool> m_pool;
    std::atomic<bool> m_cancelled{false};
    QTimer *m_removalTimer = nullptr;

    bool m_busy = false;
    bool m_verifying = false;
    bool m_autoVerifyArmed = false;
    std::function<bool()> m_cleanupVisible;
    std::optional<LegacyImportPlan> m_quickStartPlan;
    std::optional<LegacyDetection> m_removalDetection;
    std::optional<LegacyImportEvidence> m_removalEvidence;
};

}  // namespace sky

Q_DECLARE_METATYPE(sky::LegacyQuickStartState)
Q_DECLARE_METATYPE(sky::LegacyCleanupPresentation)
Q_DECLARE_METATYPE(sky::LegacyImportResult)
Q_DECLARE_METATYPE(sky::RemovalOutcome)
