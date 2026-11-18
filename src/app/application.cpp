#include "app/application.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QPointer>
#include <QThreadPool>

#include "app/paths.hpp"
#include "platform/file_dialog.hpp"
#include "app/player_controller.hpp"
#include "app/update_controller.hpp"
#include "app/version.hpp"
#include "core/logger.hpp"
#include "core/string_utils.hpp"
#include "crash/crash_handler.hpp"
#include "services/library/chromium_local_storage.hpp"
#include "services/settings/config_service.hpp"
#include "services/library/favorites_service.hpp"
#include "services/library/library_service.hpp"
#include "services/playback/playback_service.hpp"
#include "services/library/playlist_service.hpp"
#include "services/library/recent_service.hpp"
#include "services/settings/shortcut_service.hpp"
#include "services/store/store_service.hpp"
#include "services/notifications/toast_service.hpp"
#include "services/settings/ui_state_service.hpp"
#include "services/update/update_service.hpp"
#include "ui/main_window/main_window.hpp"
#include "platform/tray_notification.hpp"
#include "ui/editor_window/editor_window.hpp"
#include "ui/main_window/changelog_panel.hpp"
#include "ui/settings_window/settings_window.hpp"
#include "app/theme_controller.hpp"
#include "ui/main_window/quick_start.hpp"
#include "services/settings/theme_library_service.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr const char *kSettingsWarning =
    "When opening the settings, you will not be able to use shortcuts, please turn off the settings to use the shortcut!";

}  // namespace

Application::Application()
    : m_config(std::make_unique<ConfigService>(Paths::configDirectory())),
      m_uiState(std::make_unique<UiStateService>(Paths::dataDirectory())),
      m_favorites(std::make_unique<FavoritesService>(*m_uiState)),
      m_recent(std::make_unique<RecentService>(*m_uiState)),
      m_playlists(std::make_unique<PlaylistService>(Paths::dataDirectory())),
      m_library(std::make_unique<LibraryService>(Paths::dataDirectory(), *m_playlists)),
      m_toasts(std::make_unique<ToastService>()),
      m_playback(std::make_unique<PlaybackService>(*m_config)),
      m_shortcuts(std::make_unique<ShortcutService>(*m_config)),
      m_store(std::make_unique<StoreService>()),
      m_updates(std::make_unique<UpdateService>(kAppVersion, Paths::appDirectory(), Paths::configDirectory())),
      m_themeLibrary(std::make_unique<ThemeLibraryService>(Paths::dataDirectory())),
      m_themes(std::make_unique<ThemeController>(*m_uiState, *m_config, *m_themeLibrary))
{
    m_config->load();
    m_uiState->load();
    m_playlists->load();
    m_library->load();
    m_themeLibrary->load();
    m_themes->applyStartup();
    m_player = std::make_unique<PlayerController>(*m_library, *m_playback, *m_recent, *m_config, *m_toasts);
    m_mainWindow = std::make_unique<MainWindow>(
        MainWindowServices{*m_library, *m_favorites, *m_recent, *m_playlists, *m_store, *m_config, *m_player});
    wire();
}

Application::~Application()
{
    if (m_settings) {
        m_settings->disconnect(this);
        delete m_settings;
    }
    delete m_editor;
    m_shortcuts->unregisterAll();
    m_playback->shutdown();
    m_library->waitForBackgroundWork();
    m_mainWindow.reset();

    Logger::info("app", "shutdown");
}

void Application::wire()
{
    MainWindow *window = m_mainWindow.get();
    connect(m_toasts.get(), &ToastService::toastRequested, window, &MainWindow::showToast);
    m_tray = new TrayNotification(this);
    m_updateController = new UpdateController(*m_updates, *m_toasts, *m_tray, this);
    connect(m_updateController, &UpdateController::restartRequested, this, &Application::requestRelaunch);
    connect(m_tray, &TrayNotification::clicked, this, [this] {
        if (m_updates->isReady()) {
            requestRelaunch();
        }
    });
    m_changelog = new ChangelogPanel(window);
    connect(m_changelog, &Overlay::closed, this, [this] { m_updates->markChangelogShown(); });
    window->installEventFilter(this);
    connect(m_library.get(), &LibraryService::importFinished, window, [window](const ImportSummary &summary) {
        window->showToast(LibraryService::importToast(summary));
    });
    connect(m_shortcuts.get(), &ShortcutService::triggered, window, &MainWindow::handleShortcut);
    connect(m_shortcuts.get(), &ShortcutService::registrationFailed, this, [](const QString &accelerator, const QString &reason) {
        Logger::warning("shortcut", fromQString(accelerator + QStringLiteral(": ") + reason));
    });
    connect(window, &MainWindow::themeToggleRequested, this, &Application::toggleTheme);
    connect(window, &MainWindow::shortcutsSuspended, this, [this](bool suspended) {
        if (suspended) {
            m_shortcuts->suspend();
        } else {
            m_shortcuts->resume();
        }
    });
    connect(window, &MainWindow::settingsRequested, this, &Application::openSettings);
    connect(window, &MainWindow::editorRequested, this, &Application::openEditor);
    connect(m_playback.get(), &PlaybackService::started, window, [this, window] {
        if (m_config->panel().minimizeOnPlay) {
            window->showMinimized();
        }
    });
    connect(m_playback.get(), &PlaybackService::started, this, &Application::updateCrashState);
    connect(m_playback.get(), &PlaybackService::stopped, this, &Application::updateCrashState);
    connect(m_config.get(), &ConfigService::changed, this, &Application::updateCrashState);
    connect(m_library.get(), &LibraryService::changed, this, &Application::updateCrashState);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &Application::updateCrashState);
    updateCrashState();
}

void Application::updateCrashState()
{
    CrashState *state = CrashHandler::state();
    if (!state) {
        return;
    }
    const PanelConfig panel = m_config->panel();
    state->playbackMode = panel.playbackMode == PlaybackMode::Strict ? 1 : 0;
    state->playing = m_playback->isPlaying() ? 1 : 0;
    state->speed = m_config->speed();
    state->longPress = panel.longPressMode ? 1 : 0;
    state->theme = ThemeManager::instance().kind() == AppTheme::Dark ? 1 : 0;
    state->customKeyboard = m_config->keyboard().customKeyboard ? 1 : 0;
    state->librarySize = m_library->size();
    state->noteCount = static_cast<std::int32_t>(m_player->target().notes.size());
}

void Application::showPendingChangelog()
{
    const auto info = m_updates->pendingChangelog();
    if (!info || info->newVersion.empty()) {
        return;
    }
    if (compareVersions(info->newVersion, m_updates->currentVersion()) != 0) {
        Logger::warning("update", "v" + info->newVersion + " was downloaded but v" + m_updates->currentVersion() +
                                      " is running, so the update did not install");
        m_updates->markChangelogShown();
        return;
    }
    const QString version = toQString(info->newVersion);
    if (!info->notes.empty()) {
        m_changelog->present(version, toQString(info->notes));
        return;
    }
    QPointer<ChangelogPanel> panel(m_changelog);
    m_updates->fetchChangelog([panel, version](Result<std::vector<VersionNotes>> changelog) {
        if (!panel) {
            return;
        }
        const VersionNotes *notes = changelog ? findVersionNotes(changelog.value(), fromQString(version)) : nullptr;
        panel->present(version, notes ? toQString(notes->markdown)
                                      : QStringLiteral("You are now on version %1. The release notes are on GitHub.")
                                            .arg(version));
    });
}

bool Application::launchPendingInstaller()
{
    return m_updates->launchPendingInstaller();
}

void Application::requestRelaunch()
{
    m_relaunch = true;
    QApplication::quit();
}

bool Application::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mainWindow.get() && event->type() == QEvent::Close && m_updates->isUpdating()) {
        event->ignore();
        m_toasts->showTitled(ToastType::Error, "Update in Progress",
                             "Cannot close - application update is running (" + std::to_string(m_updates->progress()) +
                                 "%).");
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void Application::notifyLegacy(bool manual, const Toast &toast)
{
    if (manual && m_settings) {
        m_settings->showToast(toast);
    } else {
        m_mainWindow->showToast(toast);
    }
}

void Application::detectLegacyLibrary(bool manual)
{
    if (m_legacyBusy) {
        return;
    }
    m_legacyBusy = true;
    const std::set<std::string> keys = m_library->legacySongKeys();
    const std::filesystem::path own = Paths::dataDirectory();
    QPointer<Application> self(this);
    QThreadPool::globalInstance()->start([self, keys, own, manual] {
        std::optional<LegacyImportPlan> best;
        for (const LegacyInstall &install : findLegacyInstalls(own, defaultLegacySearchRoots())) {
            auto plan = planLegacyImport(install, keys, electronLocalStorageDirectory());
            if (plan && (!best || plan.value().rows.size() > best->rows.size())) {
                best = plan.value();
            }
        }
        QMetaObject::invokeMethod(
            self.data(),
            [self, best, manual] {
                if (self) {
                    self->onLegacyDetected(best, manual);
                }
            },
            Qt::QueuedConnection);
    });
}

void Application::onLegacyDetected(std::optional<LegacyImportPlan> plan, bool manual)
{
    m_legacyBusy = false;
    if (m_legacyForQuickStart && m_quickStart && m_quickStart->isOpen()) {
        m_legacyForQuickStart = false;
        m_uiState->setLegacyImportOffered();
        if (!plan) {
            m_quickStart->setLegacyState(QuickStart::LegacyState::NotFound);
        } else if (plan->rows.empty() && plan->extras.empty()) {
            m_quickStart->setLegacyState(QuickStart::LegacyState::UpToDate);
        } else {
            m_quickStartLegacy = *plan;
            m_quickStart->setLegacyState(QuickStart::LegacyState::Found, static_cast<int>(plan->rows.size()),
                                         static_cast<int>(plan->extras.playlists.size()));
        }
        return;
    }
    m_legacyForQuickStart = false;
    if (!plan) {
        if (manual) {
            notifyLegacy(true, Toast{ToastType::Warning, {}, "No previous version of Sky Auto Piano was found.",
                                     kDefaultToastMs});
            pickLegacyFolder();
        }
        return;
    }
    if (plan->rows.empty() && plan->extras.empty()) {
        if (manual) {
            notifyLegacy(true, Toast{ToastType::Info, {},
                                     "All songs from the previous version are already in your library.",
                                     kDefaultToastMs});
        } else {
            m_uiState->setLegacyImportOffered();
        }
        return;
    }
    if (manual) {
        importLegacy(*plan, true);
        return;
    }
    m_uiState->setLegacyImportOffered();
    const LegacyImportPlan accepted = *plan;
    const auto songs = accepted.rows.size();
    const auto playlists = accepted.extras.playlists.size();
    m_mainWindow->confirm(
        QStringLiteral("Found a previous version of Sky Auto Piano with %1 %2 and %3 %4. Import your songs, "
                       "playlists, favorites and settings into this version?")
            .arg(songs)
            .arg(songs == 1 ? QStringLiteral("song") : QStringLiteral("songs"))
            .arg(playlists)
            .arg(playlists == 1 ? QStringLiteral("playlist") : QStringLiteral("playlists")),
        [this, accepted] { importLegacy(accepted, false); });
}

void Application::pickLegacyFolder()
{
    QWidget *parent = m_settings ? static_cast<QWidget *>(m_settings) : m_mainWindow.get();
    const auto folder = chooseFolder(parent, QStringLiteral("Select the previous Sky Auto Piano folder"));
    if (!folder) {
        if (m_legacyForQuickStart && m_quickStart && m_quickStart->isOpen()) {
            m_legacyForQuickStart = false;
            if (m_quickStart->legacyState() == QuickStart::LegacyState::Searching) {
                m_quickStart->setLegacyState(QuickStart::LegacyState::NotFound);
            }
        }
        return;
    }
    m_legacyBusy = true;
    const std::set<std::string> keys = m_library->legacySongKeys();
    QPointer<Application> self(this);
    QThreadPool::globalInstance()->start([self, keys, folder = *folder] {
        std::optional<LegacyImportPlan> plan;
        if (const auto install = inspectLegacyFolder(folder)) {
            if (auto planned = planLegacyImport(*install, keys, electronLocalStorageDirectory())) {
                plan = planned.value();
            }
        }
        QMetaObject::invokeMethod(
            self.data(),
            [self, plan] {
                if (!self) {
                    return;
                }
                self->m_legacyBusy = false;
                if (!plan) {
                    if (self->m_legacyForQuickStart && self->m_quickStart && self->m_quickStart->isOpen()) {
                        self->m_legacyForQuickStart = false;
                        self->m_quickStart->setLegacyState(
                            QuickStart::LegacyState::NotFound, 0, 0,
                            QStringLiteral("That folder does not contain songs from Sky Auto Piano. Try another "
                                           "folder."));
                        return;
                    }
                    self->notifyLegacy(true, Toast{ToastType::Error, {},
                                                   "That folder does not contain songs from Sky Auto Piano.",
                                                   kDefaultToastMs});
                    return;
                }
                self->onLegacyDetected(plan, true);
            },
            Qt::QueuedConnection);
    });
}

void Application::importLegacy(const LegacyImportPlan &plan, bool manual)
{
    m_legacyBusy = true;
    QPointer<Application> self(this);
    m_library->importLegacyRows(plan.install.dataDirectory, plan.rows, plan.alreadyPresent,
                                [self, manual, plan](LegacyImportSummary summary) {
                                    if (!self) {
                                        return;
                                    }
                                    self->m_legacyBusy = false;
                                    std::string text = "Imported " + self->applyLegacyExtras(plan, summary) +
                                                       " from the previous version.";
                                    if (summary.failed > 0) {
                                        text += " " + std::to_string(summary.failed) + " songs could not be copied.";
                                    }
                                    self->notifyLegacy(manual, Toast{ToastType::Success, {}, text, kDefaultToastMs});
                                    if (self->m_quickStart &&
                                        self->m_quickStart->legacyState() == QuickStart::LegacyState::Importing) {
                                        self->m_quickStart->setLegacyState(QuickStart::LegacyState::Imported, 0, 0,
                                                                           toQString(text));
                                    }
                                    if (self->m_settings) {
                                        self->m_settings->reloadForm();
                                    }
                                });
}

std::string Application::applyLegacyExtras(const LegacyImportPlan &plan, const LegacyImportSummary &summary)
{
    const LegacyExtras &extras = plan.extras;
    std::map<std::string, std::string> nativeByKey;
    for (const SheetEntry &entry : m_library->entries()) {
        nativeByKey.emplace(legacySongKey(entry.json()), entry.payloadFile());
    }
    std::map<std::string, std::string> payloadFor = summary.payloadMap;
    for (const OrderedJson &row : plan.allRows) {
        const std::string legacyFile = row.value("keyMap", std::string());
        if (!payloadFor.contains(legacyFile)) {
            if (const auto found = nativeByKey.find(legacySongKey(row)); found != nativeByKey.end()) {
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
        if (legacy.contains("sheetKeys") && legacy["sheetKeys"].is_array()) {
            for (const OrderedJson &key : legacy["sheetKeys"]) {
                if (key.is_string()) {
                    if (const auto mapped = payloadFor.find(key.get<std::string>()); mapped != payloadFor.end()) {
                        keys.push_back(mapped->second);
                    }
                }
            }
        }
        const std::string name = PlaylistService::normalizeName(legacy["name"].get<std::string>());
        std::string id;
        for (const Playlist &existing : m_playlists->playlists()) {
            if (PlaylistService::normalizeName(existing.name) == name) {
                id = existing.id;
            }
        }
        if (id.empty()) {
            auto created = m_playlists->create(name);
            if (!created) {
                continue;
            }
            id = created.value().id;
        }
        if (!keys.empty()) {
            (void)m_playlists->addSheets(id, keys);
        }
        ++playlists;
    }

    int favorites = 0;
    for (const std::string &name : extras.favorites) {
        if (!m_favorites->isFavorite(name)) {
            m_favorites->toggle(name);
            ++favorites;
        }
    }
    if (m_recent->names().empty()) {
        for (auto it = extras.recentPlays.rbegin(); it != extras.recentPlays.rend(); ++it) {
            m_recent->add(*it);
        }
    }

    bool settings = false;
    if (extras.config) {
        if (auto merged = ConfigService::normalizeImported(m_config->raw(), *extras.config)) {
            if (ConfigService::validateShortcuts(ConfigService::fromJson(merged.value()).shortcut) &&
                m_config->replace(merged.value())) {
                settings = true;
                m_shortcuts->registerAll();
            }
        }
    }
    if (extras.theme && !m_themes->followsSystem()) {
        applyTheme(*extras.theme == "dark" ? AppTheme::Dark : AppTheme::Light);
    }

    const auto count = [](std::size_t n, const char *one, const char *many) {
        return std::to_string(n) + " " + (n == 1 ? one : many);
    };
    std::vector<std::string> parts = {count(static_cast<std::size_t>(summary.imported), "song", "songs")};
    if (playlists > 0) {
        parts.push_back(count(static_cast<std::size_t>(playlists), "playlist", "playlists"));
    }
    if (favorites > 0) {
        parts.push_back(count(static_cast<std::size_t>(favorites), "favorite", "favorites"));
    }
    if (settings) {
        parts.push_back("your settings");
    }
    std::string text = parts.front();
    for (std::size_t i = 1; i < parts.size(); ++i) {
        text += (i + 1 == parts.size() ? " and " : ", ") + parts[i];
    }
    return text;
}

void Application::toggleTheme()
{
    m_themes->toggle();
}

void Application::applyTheme(AppTheme theme)
{
    m_themes->setKind(theme);
}

void Application::openEditor(int libraryIndex)
{
    if (!m_editor) {
        m_editor = new EditorWindow(*m_library, m_mainWindow.get());
        m_editor->show();
    } else if (m_editor->isMinimized()) {
        m_editor->showNormal();
    }
    m_editor->openSheet(libraryIndex);
    m_editor->raise();
    m_editor->activateWindow();
}

void Application::openSettings()
{
    if (m_settings) {
        if (m_settings->isMinimized()) {
            m_settings->showNormal();
        }
        m_settings->raise();
        m_settings->activateWindow();
        return;
    }
    m_toasts->show(ToastType::Warning, kSettingsWarning);
    m_shortcuts->suspend();
    m_settings = new SettingsWindow(SettingsServices{*m_config, *m_updates, *m_themes}, m_mainWindow.get());
    connect(m_settings, &QObject::destroyed, this, [this] { m_shortcuts->resume(); });
    connect(m_settings, &SettingsWindow::themeImported, this, &Application::applyTheme);
    connect(m_settings, &SettingsWindow::importPreviousRequested, this, [this] { detectLegacyLibrary(true); });
    connect(m_settings, &SettingsWindow::quickStartRequested, this, [this] {
        m_mainWindow->bringToFront();
        showQuickStart();
    });
    connect(m_settings, &SettingsWindow::updateAvailable, this,
            [this](const VersionCheck &check) { m_updateController->offer(m_settings, check); });
    m_settings->show();
    m_settings->activateWindow();
}

void Application::start(bool afterCrash)
{
    m_mainWindow->show();
    m_mainWindow->setFocus();
    m_shortcuts->registerAll();
    if (m_library->size() > 0) {
        m_player->selectLocal(0, false);
    }
    Logger::info("app", "started library=" + std::to_string(m_library->size()));
    m_updates->cleanupPendingFiles();
    m_updateController->start(m_mainWindow.get(), [this] { return !m_config->value().update.blockUpdate; });
    showPendingChangelog();
    if (!m_uiState->quickStartDone()) {
        showQuickStart();
    } else if (!m_uiState->legacyImportOffered()) {
        detectLegacyLibrary(false);
    }
    if (afterCrash) {
        m_toasts->show(ToastType::Error, "Sky Auto Piano closed unexpectedly last time. A crash report was saved.",
                       kDefaultToastMs);
    }
}

void Application::showQuickStart()
{
    if (!m_quickStart) {
        m_quickStart = new QuickStart(*m_themes, *m_config, m_mainWindow.get());
        connect(m_quickStart, &QuickStart::finished, this, [this] { m_uiState->setQuickStartDone(); });
        connect(m_quickStart, &QuickStart::legacyImportRequested, this, [this] {
            if (m_quickStartLegacy) {
                const LegacyImportPlan plan = *m_quickStartLegacy;
                m_quickStartLegacy.reset();
                importLegacy(plan, false);
            }
        });
        connect(m_quickStart, &QuickStart::legacyFolderRequested, this, [this] {
            if (m_legacyBusy) {
                return;
            }
            m_legacyForQuickStart = true;
            m_quickStart->setLegacyState(QuickStart::LegacyState::Searching);
            pickLegacyFolder();
        });
    }
    if (!m_quickStart->isOpen()) {
        m_quickStart->start();
        m_quickStartLegacy.reset();
        m_legacyForQuickStart = true;
        detectLegacyLibrary(false);
    }
}

void Application::activate()
{
    m_mainWindow->bringToFront();
}

}  // namespace sky
