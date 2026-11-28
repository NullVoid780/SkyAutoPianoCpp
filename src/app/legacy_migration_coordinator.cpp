#include "app/legacy_migration_coordinator.hpp"

#include <QMetaObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <utility>

#include "core/json.hpp"
#include "core/logger.hpp"
#include "core/paths.hpp"
#include "core/string_utils.hpp"
#include "core/text_file.hpp"
#include "core/translator.hpp"
#include "services/library/chromium_local_storage.hpp"
#include "services/library/favorites_service.hpp"
#include "services/library/library_service.hpp"
#include "services/library/playlist_service.hpp"
#include "services/library/recent_service.hpp"
#include "services/settings/config_service.hpp"
#include "services/settings/shortcut_service.hpp"
#include "services/settings/ui_state_service.hpp"
#include "ui/controllers/theme_controller.hpp"

namespace sky {

namespace {

QString songCount(std::size_t count) {
    return count == 1 ? tr("1 song") : tr("%1 songs").arg(count);
}

QString playlistCount(std::size_t count) {
    return count == 1 ? tr("1 playlist") : tr("%1 playlists").arg(count);
}

QString joinList(const QStringList &items) {
    if (items.isEmpty()) {
        return {};
    }
    QString text = items.front();
    for (qsizetype i = 1; i < items.size(); ++i) {
        text = (i + 1 == items.size() ? tr("%1 and %2") : tr("%1, %2")).arg(text, items[i]);
    }
    return text;
}

}  // namespace

LegacyCoordinatorEnvironment LegacyCoordinatorEnvironment::production() {
    LegacyCoordinatorEnvironment env;
    env.appDirectory = Paths::appDirectory();
    env.dataDirectory = Paths::dataDirectory();
    env.configDirectory = Paths::configDirectory();
    env.localStorageDirectory = electronLocalStorageDirectory();
    env.searchRoots = defaultLegacySearchRoots();
    env.useRegistryHints = true;
    env.removalSystem = makeWin32LegacyRemovalSystem();
    env.libraryIntactCheck = [dataDir = env.dataDirectory] {
        const auto list = readTextFile(dataDir / "listSheet.json");
        if (!list) {
            return false;
        }
        const auto parsed = parseJson<OrderedJson>(list.value());
        return parsed && parsed.value().is_array();
    };
    return env;
}

LegacyImportResult
aggregateLegacyImportResult(const LegacyImportPlan &plan, const LegacyImportSummary &summary,
                            const LegacyExtrasOutcome &outcome, const std::string &fingerprint,
                            const std::filesystem::path &destDataDirectory, bool statePersisted) {
    LegacyImportResult result;
    result.sourceFingerprint = fingerprint;
    result.sheets.planned = static_cast<int>(plan.rows.size()) + summary.alreadyPresent +
                            static_cast<int>(plan.conflicts.size());
    result.sheets.copied = summary.imported;
    result.sheets.alreadyPresent = summary.alreadyPresent;
    result.sheets.conflicts = static_cast<int>(plan.conflicts.size());
    result.sheets.failed = summary.failed + plan.missingPayloads;
    result.playlists.created = outcome.playlistsCreated;
    result.playlists.merged = outcome.playlistsMerged;
    result.playlists.referencesRemapped = outcome.playlistReferencesRemapped;
    result.playlists.missingReferences = outcome.playlistMissingReferences;
    result.config.applied = outcome.settingsApplied;
    result.config.rejected = outcome.settingsRejected;
    result.extras.favoritesImported = outcome.favoritesImported;
    result.extras.favoritesAlreadyPresent = outcome.favoritesAlreadyPresent;
    result.extras.recentImported = outcome.recentImported;
    result.extras.recentSkipped = outcome.recentSkipped;
    result.extras.themeApplied = outcome.themeApplied;
    result.extras.themeSkipped = outcome.themeSkipped;

    std::vector<std::pair<std::string, std::string>> expected;
    std::vector<std::string> referenced;
    for (const auto &[sourceFile, destFile] : summary.payloadMap) {
        expected.emplace_back(sourceFile, destFile);
        referenced.push_back(destFile);
    }
    const LegacyVerification payloadsCheck =
        verifyImportedSheets(plan.install.dataDirectory, destDataDirectory, expected, {});
    const LegacyVerification referencesCheck =
        verifyImportedSheets(plan.install.dataDirectory, destDataDirectory, {}, referenced);
    result.sheets.validated = std::max(0, static_cast<int>(expected.size()) - payloadsCheck.failed);
    result.verificationChecked = payloadsCheck.checked + referencesCheck.checked;
    result.verificationFailed = payloadsCheck.failed + referencesCheck.failed;
    for (const std::string &failure : payloadsCheck.failures) {
        result.warnings.push_back(failure);
    }
    for (const std::string &failure : referencesCheck.failures) {
        result.warnings.push_back(failure);
    }
    if (result.sheets.conflicts > 0) {
        result.warnings.push_back("sheets skipped as conflicts: " +
                                  std::to_string(result.sheets.conflicts));
    }
    if (!summary.indexCommitted) {
        result.warnings.push_back(summary.errorMessage.empty() ? "library index persistence failed"
                                                               : summary.errorMessage);
    }
    if (!statePersisted) {
        result.warnings.push_back("migration state could not be persisted");
    }
    const bool verified = statePersisted && summary.indexCommitted && payloadsCheck.verified &&
                          referencesCheck.verified && summary.failed == 0 &&
                          plan.missingPayloads == 0 && plan.conflicts.empty() &&
                          outcome.playlistMissingReferences == 0 && !outcome.settingsRejected;
    result.verificationStatus = verified ? "verified" : "needs_review";
    return result;
}

LegacyImportEvidence
buildLegacyRemovalEvidence(const UiStateService &uiState, const LegacyInstallInfo &install,
                           const std::optional<LegacyImportEvidence> &sessionEvidence) {
    LegacyImportEvidence evidence;
    evidence.migrationStatus = uiState.legacyMigrationStatus();
    evidence.sourceFingerprint =
        uiState.legacyMigration().value("sourceFingerprint", std::string());
    evidence.legacyRemoved = uiState.legacyRemoved();
    const OrderedJson result = uiState.legacyMigration().value("result", OrderedJson::object());
    const OrderedJson sheets = result.value("sheets", OrderedJson::object());
    const OrderedJson playlists = result.value("playlists", OrderedJson::object());
    const OrderedJson config = result.value("config", OrderedJson::object());
    const OrderedJson verification = result.value("verification", OrderedJson::object());
    evidence.hasSeriousFailures =
        sheets.value("failed", 0) > 0 || sheets.value("conflicts", 0) > 0 ||
        playlists.value("missingReferences", 0) > 0 || config.value("rejected", false) ||
        verification.value("failed", 0) > 0;
    // A reconciliation result earned earlier in this session stays valid for
    // the same source; a different source always has to earn its own.
    if (sessionEvidence && sessionEvidence->sourceFingerprint == install.sourceFingerprint) {
        evidence.reVerified = sessionEvidence->reVerified;
        evidence.reVerificationFailed = sessionEvidence->reVerificationFailed;
    }
    return evidence;
}

LegacyMigrationCoordinator::LegacyMigrationCoordinator(
    LibraryService &library, PlaylistService &playlists, FavoritesService &favorites,
    RecentService &recent, ConfigService &config, ShortcutService &shortcuts,
    ThemeController &themes, UiStateService &uiState, LegacyCoordinatorEnvironment env,
    QObject *parent)
    : QObject(parent), m_library(library), m_playlists(playlists), m_favorites(favorites),
      m_recent(recent), m_config(config), m_shortcuts(shortcuts), m_themes(themes),
      m_uiState(uiState), m_env(std::move(env)), m_pool(std::make_unique<QThreadPool>()),
      m_removalTimer(new QTimer(this)) {
    qRegisterMetaType<sky::LegacyQuickStartState>();
    qRegisterMetaType<sky::LegacyCleanupPresentation>();
    qRegisterMetaType<sky::LegacyImportResult>();
    qRegisterMetaType<sky::RemovalOutcome>();

    m_pool->setMaxThreadCount(2);
    if (m_env.removalSystem) {
        m_removal = std::make_unique<LegacyRemovalService>(m_env.removalSystem, m_env.appDirectory,
                                                           m_env.dataDirectory,
                                                           std::move(m_env.libraryIntactCheck));
    }
    m_removalTimer->setInterval(1500);
    connect(m_removalTimer, &QTimer::timeout, this, &LegacyMigrationCoordinator::pollRemoval);
}

LegacyMigrationCoordinator::~LegacyMigrationCoordinator() {
    shutdown();
}

void LegacyMigrationCoordinator::shutdown() {
    m_cancelled.store(true);
    if (m_removalTimer) {
        m_removalTimer->stop();
    }
    if (m_pool) {
        m_pool->waitForDone();
    }
}

std::map<std::string, std::string> LegacyMigrationCoordinator::existingPayloads() const {
    std::map<std::string, std::string> payloads;
    for (const SheetEntry &entry : m_library.entries()) {
        payloads.emplace(legacySongKey(entry.json()), entry.payloadFile());
    }
    return payloads;
}

void LegacyMigrationCoordinator::detect(bool manual, bool forQuickStart) {
    if (m_busy || m_cancelled.load()) {
        return;
    }
    m_busy = true;
    const std::set<std::string> keys = m_library.legacySongKeys();
    const std::map<std::string, std::string> payloads = existingPayloads();
    const std::filesystem::path own = m_env.dataDirectory;
    const std::filesystem::path localStorage = m_env.localStorageDirectory;
    const std::vector<std::filesystem::path> roots = m_env.searchRoots;
    const bool useRegistry = m_env.useRegistryHints;
    QPointer<LegacyMigrationCoordinator> self(this);
    m_pool->start(
        [self, keys, payloads, own, localStorage, roots, useRegistry, manual, forQuickStart] {
            if (!self || self->m_cancelled.load()) {
                return;
            }
            std::optional<LegacyImportPlan> best;
            for (const LegacyInstall &install : findLegacyInstalls(own, roots, useRegistry)) {
                if (!self || self->m_cancelled.load()) {
                    return;
                }
                auto plan = planLegacyImport(install, keys, localStorage, own, payloads);
                if (plan && (!best || plan.value().rows.size() > best->rows.size())) {
                    best = plan.value();
                }
            }
            QMetaObject::invokeMethod(
                self.data(),
                [self, best, manual, forQuickStart] {
                    if (self && !self->m_cancelled.load()) {
                        self->onDetected(best, manual, forQuickStart);
                    }
                },
                Qt::QueuedConnection);
        });
}

void LegacyMigrationCoordinator::onDetected(std::optional<LegacyImportPlan> plan, bool manual,
                                            bool forQuickStart) {
    m_busy = false;
    if (forQuickStart) {
        m_uiState.setLegacyImportOffered();
        if (!plan) {
            m_quickStartPlan.reset();
            emit quickStartStateChanged(LegacyQuickStartState::NotFound, 0, 0, {});
        } else if (plan->rows.empty() && plan->extras.empty()) {
            m_quickStartPlan.reset();
            emit quickStartStateChanged(LegacyQuickStartState::UpToDate, 0, 0, {});
        } else {
            m_quickStartPlan = *plan;
            emit quickStartStateChanged(LegacyQuickStartState::Found,
                                        static_cast<int>(plan->rows.size()),
                                        static_cast<int>(plan->extras.playlists.size()), {});
        }
        return;
    }
    if (!plan) {
        if (manual) {
            emit toastRequested(Toast{ToastType::Warning,
                                      {},
                                      trUtf8("No previous version of Sky Auto Piano was found."),
                                      kDefaultToastMs},
                                true);
            emit folderPickRequested();
        }
        return;
    }
    if (plan->rows.empty() && plan->extras.empty()) {
        if (manual) {
            emit toastRequested(
                Toast{ToastType::Info,
                      {},
                      trUtf8("All songs from the previous version are already in your library."),
                      kDefaultToastMs},
                true);
        } else {
            m_uiState.setLegacyImportOffered();
        }
        return;
    }
    (void)m_uiState.setLegacyMigration("available", legacySourceFingerprint(plan->install), {});
    if (manual) {
        importPlan(*plan, true);
        return;
    }
    m_uiState.setLegacyImportOffered();
    const LegacyImportPlan accepted = *plan;
    const QString prompt =
        tr("Found a previous version of Sky Auto Piano with %1 and %2. Import your songs, "
           "playlists, favorites and "
           "settings into this version?")
            .arg(songCount(accepted.rows.size()), playlistCount(accepted.extras.playlists.size()));
    emit autoImportPromptRequested(accepted, prompt);
}

void LegacyMigrationCoordinator::cancelQuickStartFolderPick() {
    emit quickStartStateChanged(LegacyQuickStartState::NotFound, 0, 0, {});
}

void LegacyMigrationCoordinator::inspectFolder(const std::filesystem::path &folder,
                                               bool forQuickStart) {
    if (m_busy || m_cancelled.load()) {
        return;
    }
    m_busy = true;
    const std::set<std::string> keys = m_library.legacySongKeys();
    const std::map<std::string, std::string> payloads = existingPayloads();
    const std::filesystem::path own = m_env.dataDirectory;
    const std::filesystem::path localStorage = m_env.localStorageDirectory;
    QPointer<LegacyMigrationCoordinator> self(this);
    m_pool->start([self, keys, payloads, own, localStorage, folder, forQuickStart] {
        if (!self || self->m_cancelled.load()) {
            return;
        }
        std::optional<LegacyImportPlan> plan;
        if (const auto install = inspectLegacyFolder(folder)) {
            if (auto planned = planLegacyImport(*install, keys, localStorage, own, payloads)) {
                plan = planned.value();
            }
        }
        QMetaObject::invokeMethod(
            self.data(),
            [self, plan, forQuickStart] {
                if (!self || self->m_cancelled.load()) {
                    return;
                }
                self->m_busy = false;
                if (!plan) {
                    if (forQuickStart) {
                        emit self->quickStartStateChanged(
                            LegacyQuickStartState::NotFound, 0, 0,
                            tr("That folder does not contain songs from Sky Auto Piano. Try "
                               "another folder."));
                        return;
                    }
                    emit self->toastRequested(
                        Toast{ToastType::Error,
                              {},
                              trUtf8("That folder does not contain songs from Sky Auto Piano."),
                              kDefaultToastMs},
                        true);
                    return;
                }
                self->onDetected(plan, true, forQuickStart);
            },
            Qt::QueuedConnection);
    });
}

bool LegacyMigrationCoordinator::importPendingQuickStartPlan() {
    if (!m_quickStartPlan || m_busy) {
        return false;
    }
    const LegacyImportPlan plan = *m_quickStartPlan;
    m_quickStartPlan.reset();
    importPlan(plan, false);
    return true;
}

void LegacyMigrationCoordinator::importPlan(const LegacyImportPlan &plan, bool manual) {
    if (m_busy || m_cancelled.load()) {
        return;
    }
    m_busy = true;
    const std::string fingerprint = legacySourceFingerprint(plan.install);
    (void)m_uiState.setLegacyMigration("importing", fingerprint, {});
    // Snapshot the pre-existing destination files before anything mutates
    // them, so a partial merge stays recoverable. The legacy source itself
    // is only ever read.
    (void)snapshotDestinationFiles(m_env.dataDirectory, m_env.configDirectory, fingerprint);
    QPointer<LegacyMigrationCoordinator> self(this);
    m_library.importLegacyRows(
        plan.install.dataDirectory, plan.rows, plan.alreadyPresent,
        [self, manual, plan, fingerprint](LegacyImportSummary summary) {
            if (!self || self->m_cancelled.load()) {
                return;
            }
            self->m_busy = false;
            const LegacyExtrasOutcome outcome = self->applyExtras(plan, summary);
            LegacyImportResult result = aggregateLegacyImportResult(
                plan, summary, outcome, fingerprint, self->m_env.dataDirectory, true);
            const bool persisted = self->m_uiState.setLegacyMigration(result.verificationStatus,
                                                                      fingerprint, result.toJson());
            if (!persisted) {
                result = aggregateLegacyImportResult(plan, summary, outcome, fingerprint,
                                                     self->m_env.dataDirectory, false);
            }
            const bool verified = result.verificationStatus == "verified";
            Logger::info("app", "legacy import result status=" + result.verificationStatus +
                                    " copied=" + std::to_string(result.sheets.copied) +
                                    " conflicts=" + std::to_string(result.sheets.conflicts) +
                                    " failed=" + std::to_string(result.sheets.failed) +
                                    " validated=" + std::to_string(result.sheets.validated));

            QString message =
                tr("Imported %1 from the previous version.").arg(toQString(outcome.text));
            if (summary.failed > 0) {
                message +=
                    QLatin1Char(' ') +
                    (summary.failed == 1 ? tr("1 song could not be copied.")
                                         : tr("%1 songs could not be copied.").arg(summary.failed));
            }
            if (!plan.conflicts.empty()) {
                message += QLatin1Char(' ') +
                           tr("%1 song(s) share a name with a different song already in "
                              "your library and were skipped.")
                               .arg(static_cast<int>(plan.conflicts.size()));
            }
            if (!verified) {
                message +=
                    QLatin1Char(' ') +
                    tr("Some items need attention - you can retry the import from Quick Start.");
            }
            const std::string text = fromQString(message);
            emit self->toastRequested(
                Toast{
                    verified ? ToastType::Success : ToastType::Warning, {}, text, kDefaultToastMs},
                manual);
            emit self->quickStartStateChanged(LegacyQuickStartState::Imported, 0, 0,
                                              toQString(text));
            emit self->importCompleted(result, manual, message);
            self->refreshCleanup();
        });
}

LegacyExtrasOutcome LegacyMigrationCoordinator::applyExtras(const LegacyImportPlan &plan,
                                                            const LegacyImportSummary &summary) {
    LegacyExtrasOutcome outcome;
    const LegacyExtras &extras = plan.extras;
    std::map<std::string, std::string> nativeByKey;
    for (const SheetEntry &entry : m_library.entries()) {
        nativeByKey.emplace(legacySongKey(entry.json()), entry.payloadFile());
    }
    std::map<std::string, std::string> payloadFor = summary.payloadMap;
    for (const OrderedJson &row : plan.allRows) {
        const std::string legacyFile = row.value("keyMap", std::string());
        if (!payloadFor.contains(legacyFile)) {
            if (const auto found = nativeByKey.find(legacySongKey(row));
                found != nativeByKey.end()) {
                payloadFor[legacyFile] = found->second;
            }
        }
    }

    int playlists = 0;
    for (const OrderedJson &legacy : extras.playlists) {
        if (!legacy.is_object() || !legacy.contains("name") || !legacy["name"].is_string()) {
            continue;
        }
        std::vector<std::string> keys;
        int legacyKeys = 0;
        if (legacy.contains("sheetKeys") && legacy["sheetKeys"].is_array()) {
            for (const OrderedJson &key : legacy["sheetKeys"]) {
                if (key.is_string()) {
                    ++legacyKeys;
                    if (const auto mapped = payloadFor.find(key.get<std::string>());
                        mapped != payloadFor.end()) {
                        keys.push_back(mapped->second);
                    }
                }
            }
        }
        outcome.playlistReferencesRemapped += static_cast<int>(keys.size());
        outcome.playlistMissingReferences += legacyKeys - static_cast<int>(keys.size());
        const std::string name = PlaylistService::normalizeName(legacy["name"].get<std::string>());
        std::string id;
        for (const Playlist &existing : m_playlists.playlists()) {
            if (PlaylistService::normalizeName(existing.name) == name) {
                id = existing.id;
            }
        }
        if (id.empty()) {
            auto created = m_playlists.create(name);
            if (!created) {
                continue;
            }
            id = created.value().id;
            ++outcome.playlistsCreated;
        } else {
            ++outcome.playlistsMerged;
        }
        if (!keys.empty()) {
            (void)m_playlists.addSheets(id, keys);
        }
        ++playlists;
    }

    for (const std::string &name : extras.favorites) {
        if (!m_favorites.isFavorite(name)) {
            m_favorites.toggle(name);
            ++outcome.favoritesImported;
        } else {
            ++outcome.favoritesAlreadyPresent;
        }
    }
    if (!extras.recentPlays.empty()) {
        if (m_recent.names().empty()) {
            for (auto it = extras.recentPlays.rbegin(); it != extras.recentPlays.rend(); ++it) {
                m_recent.add(*it);
            }
            outcome.recentImported = true;
        } else {
            outcome.recentSkipped = true;
        }
    }

    if (extras.config) {
        outcome.settingsRejected = true;
        if (auto merged = ConfigService::normalizeImported(m_config.raw(), *extras.config)) {
            if (ConfigService::validateShortcuts(
                    ConfigService::fromJson(merged.value()).shortcut) &&
                m_config.replace(merged.value())) {
                outcome.settingsApplied = true;
                outcome.settingsRejected = false;
                m_shortcuts.registerAll();
            }
        }
    }
    if (extras.theme) {
        if (!m_themes.followsSystem()) {
            m_themes.setKind(*extras.theme == "dark" ? AppTheme::Dark : AppTheme::Light);
            outcome.themeApplied = true;
        } else {
            outcome.themeSkipped = true;
        }
    }

    QStringList parts = {songCount(static_cast<std::size_t>(summary.imported))};
    if (playlists > 0) {
        parts << playlistCount(static_cast<std::size_t>(playlists));
    }
    if (outcome.favoritesImported > 0) {
        parts << (outcome.favoritesImported == 1
                      ? tr("1 favorite")
                      : tr("%1 favorites").arg(outcome.favoritesImported));
    }
    if (outcome.settingsApplied) {
        parts << tr("your settings");
    }
    outcome.text = fromQString(joinList(parts));
    return outcome;
}

void LegacyMigrationCoordinator::setCleanupVisibleProvider(std::function<bool()> provider) {
    m_cleanupVisible = std::move(provider);
}

void LegacyMigrationCoordinator::refreshCleanup() {
    if (!m_removal) {
        return;
    }
    m_removalDetection = m_removal->detectLegacyInstall();
    if (m_removalDetection->install) {
        m_removalEvidence =
            buildLegacyRemovalEvidence(m_uiState, *m_removalDetection->install, m_removalEvidence);
    } else {
        m_removalEvidence.reset();
    }
    m_autoVerifyArmed = true;
    emitCleanupPresentation();
}

QString LegacyMigrationCoordinator::legacyOldVersion() const {
    if (m_removalDetection && m_removalDetection->install) {
        return QString::fromStdString(m_removalDetection->install->legacy.version);
    }
    const std::string fingerprint =
        m_uiState.legacyMigration().value("sourceFingerprint", std::string());
    const std::size_t colon = fingerprint.find(':');
    if (colon != std::string::npos && colon > 0) {
        return QString::fromStdString(fingerprint.substr(0, colon));
    }
    return QString();
}

QString LegacyMigrationCoordinator::legacyCleanupTitle() const {
    const QString version = legacyOldVersion();
    return version.isEmpty() ? tr("Uninstall the old version")
                             : tr("Uninstall version %1 (old version)").arg(version);
}

void LegacyMigrationCoordinator::emitCleanupPresentation() {
    if (!m_removalDetection) {
        return;
    }
    const LegacyDetection &detection = *m_removalDetection;
    const bool uninstalling = m_removal->removalInProgress();
    const bool busy = uninstalling || m_verifying;
    QString description;
    bool removeEnabled = false;
    const QString version = legacyOldVersion();
    const QString alreadyRemoved = version.isEmpty()
                                       ? tr("The old version has already been removed.")
                                       : tr("Version %1 has already been removed.").arg(version);
    if (m_verifying) {
        description =
            tr("Verifying that everything from the old version is complete in this library...");
    } else if (detection.status == LegacyDetectionStatus::NotFound && m_uiState.legacyRemoved()) {
        description = alreadyRemoved;
    } else if (detection.status == LegacyDetectionStatus::NotFound) {
        description = tr("No old version was found on this PC.");
    } else if (detection.status == LegacyDetectionStatus::Ambiguous) {
        description =
            tr("More than one old installation is registered, so automatic removal is off. You can "
               "remove the old version in Windows Settings > Apps.");
    } else if (detection.status == LegacyDetectionStatus::Invalid) {
        description =
            tr("The old version's uninstaller could not be validated, so automatic removal is off. "
               "You can remove the old version in Windows Settings > Apps.");
    } else if (uninstalling) {
        description = version.isEmpty() ? tr("Uninstalling the old version...")
                                        : tr("Uninstalling version %1...").arg(version);
    } else if (m_removalEvidence) {
        const RemovalGate gate = m_removal->canOfferRemoval(*m_removalEvidence, detection);
        removeEnabled = gate.allowed;
        if (gate.allowed) {
            description =
                tr("Your data from the old version is verified in this library. This library is "
                   "independent and does not sync back to the old version.");
        } else {
            switch (gate.blockers.front()) {
            case RemovalBlocker::AlreadyRemoved:
                description = alreadyRemoved;
                break;
            case RemovalBlocker::ImportNotVerified:
                description =
                    tr("Removal unlocks after your data from the old version has been imported "
                       "and verified.");
                break;
            case RemovalBlocker::SourceMismatch:
                description =
                    tr("The verified import came from a different copy of the old version, so "
                       "removal is off.");
                break;
            case RemovalBlocker::SeriousImportIssues:
                description =
                    tr("The last import had problems that need review, so removal is off.");
                break;
            case RemovalBlocker::ReverificationRequired:
                description = tr("Your imported data needs a fresh verification before removal.");
                break;
            case RemovalBlocker::PathOverlap:
                description =
                    tr("The old and new versions share folders, so removal is off for safety.");
                break;
            case RemovalBlocker::LegacyRunning:
                description =
                    version.isEmpty()
                        ? tr("The old version is still running. Close it, then try again.")
                        : tr("Version %1 is still running. Close it, then try again.").arg(version);
                break;
            default:
                description = tr("The old version could not be validated for removal.");
                break;
            }
        }
    }
    const bool removedAndGone =
        detection.status == LegacyDetectionStatus::NotFound && m_uiState.legacyRemoved();
    const bool present =
        !removedAndGone &&
        (detection.status != LegacyDetectionStatus::NotFound || m_removalEvidence.has_value() ||
         m_uiState.legacyRemoved() || m_uiState.legacyMigrationStatus() != "not_detected");

    emit cleanupPresentationChanged(
        LegacyCleanupPresentation{legacyCleanupTitle(), description, removeEnabled, busy, present});
    maybeAutoVerify();
}

void LegacyMigrationCoordinator::armAutoVerify() {
    m_autoVerifyArmed = true;
    maybeAutoVerify();
}

void LegacyMigrationCoordinator::maybeAutoVerify() {
    if (!m_autoVerifyArmed || m_verifying || m_busy || !m_removal ||
        m_removal->removalInProgress() || !m_removalDetection || !m_removalDetection->install ||
        !m_removalEvidence) {
        return;
    }
    const bool shown = m_cleanupVisible && m_cleanupVisible();
    if (!shown || m_removalEvidence->migrationStatus != "verified") {
        return;
    }
    const auto gate = m_removal->canOfferRemoval(*m_removalEvidence, *m_removalDetection);
    if (std::find(gate.blockers.begin(), gate.blockers.end(),
                  RemovalBlocker::ReverificationRequired) == gate.blockers.end()) {
        return;
    }
    m_autoVerifyArmed = false;
    startVerification();
}

void LegacyMigrationCoordinator::startVerification() {
    if (!m_removalDetection || !m_removalDetection->install || m_verifying || m_busy ||
        m_cancelled.load()) {
        return;
    }
    const LegacyInstallInfo install = *m_removalDetection->install;
    const std::set<std::string> keys = m_library.legacySongKeys();
    const std::map<std::string, std::string> payloads = existingPayloads();
    const std::filesystem::path own = m_env.dataDirectory;
    const std::filesystem::path localStorage = m_env.localStorageDirectory;
    m_verifying = true;
    emitCleanupPresentation();
    QPointer<LegacyMigrationCoordinator> self(this);
    m_pool->start([self, install, keys, payloads, own, localStorage] {
        if (!self || self->m_cancelled.load()) {
            return;
        }
        bool verified = false;
        int failed = 0;
        auto plan = planLegacyImport(install.legacy, keys, localStorage, own, payloads);
        if (plan && plan.value().conflicts.empty() && plan.value().missingPayloads == 0 &&
            plan.value().rows.empty()) {
            std::vector<std::pair<std::string, std::string>> expected;
            std::vector<std::string> referenced;
            bool complete = true;
            for (const OrderedJson &row : dedupedLegacyRows(plan.value().allRows)) {
                const auto found = payloads.find(legacySongKey(row));
                if (found == payloads.end()) {
                    complete = false;
                    break;
                }
                expected.emplace_back(row.value("keyMap", std::string()), found->second);
                referenced.push_back(found->second);
            }
            if (complete) {
                const LegacyVerification check =
                    verifyImportedSheets(install.legacy.dataDirectory, own, expected, referenced);
                verified = check.verified;
                failed = check.failed;
            }
        }
        QMetaObject::invokeMethod(
            self.data(),
            [self, verified, failed] {
                if (!self || self->m_cancelled.load()) {
                    return;
                }
                self->m_verifying = false;
                if (self->m_removalEvidence) {
                    self->m_removalEvidence->reVerified = verified;
                    self->m_removalEvidence->reVerificationFailed = failed;
                }
                emit self->toastRequested(
                    Toast{
                        verified ? ToastType::Success : ToastType::Warning,
                        {},
                        verified
                            ? trUtf8("Your data from the old version is verified in this library.")
                            : trUtf8("Some data from the old version could not be verified. "
                                     "Import it again from Quick Start."),
                        kDefaultToastMs},
                    false);
                emit self->verificationFinished(verified, failed);
                self->emitCleanupPresentation();
            },
            Qt::QueuedConnection);
    });
}

RemovalGate LegacyMigrationCoordinator::checkRemovalGate() {
    refreshCleanup();
    if (!m_removal || !m_removalDetection || !m_removalDetection->install || !m_removalEvidence) {
        return RemovalGate{false, {RemovalBlocker::InstallNotValidated}};
    }
    const RemovalGate gate = m_removal->canOfferRemoval(*m_removalEvidence, *m_removalDetection);
    if (!gate.allowed) {
        emit toastRequested(
            Toast{ToastType::Warning,
                  {},
                  trUtf8("The old version cannot be removed yet. See the explanation in Settings."),
                  kDefaultToastMs},
            false);
    }
    return gate;
}

void LegacyMigrationCoordinator::requestRemoval() {
    refreshCleanup();
    if (!m_removal || !m_removalDetection || !m_removalDetection->install || !m_removalEvidence) {
        emit toastRequested(
            Toast{ToastType::Warning,
                  {},
                  trUtf8("The old version cannot be removed yet. See the explanation in Settings."),
                  kDefaultToastMs},
            false);
        return;
    }
    const RemovalOutcome outcome =
        m_removal->requestLegacyRemoval(*m_removalDetection->install, *m_removalEvidence);
    if (outcome.status == RemovalStatus::Launched) {
        emit toastRequested(
            Toast{ToastType::Info,
                  {},
                  trUtf8("The old version's uninstaller is open. Follow its steps to finish."),
                  kDefaultToastMs},
            false);
        m_removalTimer->start();
    } else if (outcome.status == RemovalStatus::LaunchFailed) {
        emit toastRequested(Toast{ToastType::Error,
                                  {},
                                  trUtf8("The old version's uninstaller could not be started."),
                                  kDefaultToastMs},
                            false);
    } else if (outcome.status == RemovalStatus::Blocked) {
        emit toastRequested(
            Toast{ToastType::Warning,
                  {},
                  trUtf8("The old version cannot be removed yet. See the explanation in Settings."),
                  kDefaultToastMs},
            false);
    }
    emit removalOutcomeObserved(outcome);
    refreshCleanup();
}

void LegacyMigrationCoordinator::pollRemoval() {
    if (!m_removal) {
        return;
    }
    const std::optional<RemovalOutcome> outcome = m_removal->pollRemoval();
    if (!outcome) {
        return;
    }
    m_removalTimer->stop();
    if (outcome->status == RemovalStatus::Completed) {
        m_uiState.setLegacyRemoved();
        const QString version = legacyOldVersion();
        emit toastRequested(
            Toast{ToastType::Success,
                  {},
                  version.isEmpty()
                      ? trUtf8("The old version was removed. Your library here is unchanged.")
                      : tr("Version %1 was removed. Your library here is unchanged.")
                            .arg(version)
                            .toStdString(),
                  kDefaultToastMs},
            false);
    } else if (outcome->status == RemovalStatus::Incomplete) {
        emit toastRequested(
            Toast{ToastType::Warning,
                  {},
                  trUtf8("The old version is still installed. You can try removing it again."),
                  kDefaultToastMs},
            false);
    } else {
        emit toastRequested(Toast{ToastType::Error,
                                  {},
                                  trUtf8("The old version was removed, but this library did not "
                                         "pass its check afterwards."),
                                  kDefaultToastMs},
                            false);
    }
    emit removalOutcomeObserved(*outcome);
    refreshCleanup();
}

}  // namespace sky
