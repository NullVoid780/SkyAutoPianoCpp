#include "app/application.hpp"

#include <algorithm>
#include <utility>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QLocale>
#include <QCloseEvent>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>

#include "app/paths.hpp"
#include "core/text_file.hpp"
#include "platform/file_dialog.hpp"
#include "app/player_controller.hpp"
#include "app/update_controller.hpp"
#include "app/version.hpp"
#include "core/json.hpp"
#include "core/logger.hpp"
#include "core/translator.hpp"
#include "core/string_utils.hpp"
#include "crash/crash_handler.hpp"
#include "services/library/chromium_local_storage.hpp"
#include "services/settings/config_service.hpp"
#include "services/library/favorites_service.hpp"
#include "services/library/legacy_removal.hpp"
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
#include "ui/settings_window/confirm_dialog.hpp"
#include "ui/settings_window/settings_window.hpp"
#include "app/theme_controller.hpp"
#include "app/issue_recorder.hpp"
#include "app/sky_window_watcher.hpp"
#include "crash/pending_reports.hpp"
#include "platform/shell_open.hpp"
#include "services/report/report_writer.hpp"
#include "ui/issue/issue_report_window.hpp"
#include "ui/issue/recording_bar.hpp"
#include "ui/main_window/quick_start.hpp"
#include "services/settings/theme_library_service.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr const char *kSettingsWarning =
    trNoop("When opening the settings, you will not be able to use shortcuts, please turn off the settings to use the "
           "shortcut!");

QString songCount(std::size_t count)
{
    return count == 1 ? tr("1 song") : tr("%1 songs").arg(count);
}

QString playlistCount(std::size_t count)
{
    return count == 1 ? tr("1 playlist") : tr("%1 playlists").arg(count);
}

QString joinList(const QStringList &items)
{
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
    m_removal = std::make_unique<LegacyRemovalService>(
        makeWin32LegacyRemovalSystem(), Paths::appDirectory(), Paths::dataDirectory(), [] {
            const auto list = readTextFile(Paths::dataDirectory() / "listSheet.json");
            if (!list) {
                return false;
            }
            const auto parsed = parseJson<OrderedJson>(list.value());
            return parsed && parsed.value().is_array();
        });
    m_removalTimer = new QTimer(this);
    m_removalTimer->setInterval(1500);
    connect(m_removalTimer, &QTimer::timeout, this, &Application::pollLegacyRemoval);
    m_recorder = new IssueRecorder(*m_config, *m_themes, *m_library, *m_playback, this);
    IssueRecorder::setDiagnosticsProvider([this] { return diagnostics(); });
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
    m_skyWatcher = new SkyWindowWatcher(this);
    connect(m_skyWatcher, &SkyWindowWatcher::stateChanged, this, &Application::updateSkyStatus);
    connect(m_player.get(), &PlayerController::playingChanged, this, &Application::updateSkyStatus);
    connect(m_config.get(), &ConfigService::changed, this, &Application::updateSkyStatus);
    updateSkyStatus();
    connect(m_playback.get(), &PlaybackService::stopped, this, &Application::updateCrashState);
    connect(m_config.get(), &ConfigService::changed, this, &Application::updateCrashState);
    connect(m_library.get(), &LibraryService::changed, this, &Application::updateCrashState);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &Application::updateCrashState);
    updateCrashState();
}

void Application::updateSkyStatus()
{
    const SkyWindowState state = m_skyWatcher->state();
    const bool playing = m_player->isPlaying();
    const bool pauseEnabled = m_config->panel().pauseWhenUnfocused;
    const bool hold = pauseEnabled && state != SkyWindowState::Foreground;
    m_playback->setHeld(hold);

    enum Tone { Good = 0, Warning = 1, Idle = 2 };
    QString text;
    QString tip;
    int tone = Idle;
    switch (state) {
    case SkyWindowState::Foreground:
        text = playing ? tr("Playing in Sky") : tr("Sky ready");
        tip = tr("Sky is open and in front.");
        tone = Good;
        break;
    case SkyWindowState::NotRunning:
        text = playing && hold ? tr("Waiting for Sky") : tr("Sky not open");
        tip = tr("Open Sky: Children of the Light. The song starts when the game is in front.");
        tone = playing && hold ? Warning : Idle;
        break;
    case SkyWindowState::Minimized:
    case SkyWindowState::Background:
        text = playing && hold ? tr("Paused, switch to Sky") : tr("Sky in background");
        tip = pauseEnabled ? tr("Playing pauses while another window is in front, so keys never type into it.")
                           : tr("Keys go to whatever window is in front.");
        tone = playing && hold ? Warning : Idle;
        break;
    }
    m_mainWindow->setSkyStatus(text, tone, tip);

    const bool held = playing && hold;
    if (held && !m_skyHeld) {
        m_toasts->show(ToastType::Warning,
                       state == SkyWindowState::NotRunning
                           ? trUtf8("Waiting for Sky. Open the game and the song starts when it's in front.")
                           : trUtf8("Paused because Sky isn't in front. Switch to the game and it carries on."),
                       kDefaultToastMs);
    }
    m_skyHeld = held;
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
    const std::string current = m_updates->currentVersion();
    if (const auto info = m_updates->pendingChangelog(); info && !info->newVersion.empty()) {
        if (compareVersions(info->newVersion, current) == 0) {
            presentReleaseNotes(current, info->notes);
            return;
        }
        Logger::warning("update", "v" + info->newVersion + " was downloaded but v" + current +
                                      " is running, so the update did not install");
        m_updates->markChangelogShown();
    }
    const std::string seen = m_updates->lastSeenVersion();
    const bool updated = seen.empty() ? m_uiState->quickStartDone() : compareVersions(current, seen) > 0;
    if (!updated) {
        m_updates->markVersionSeen();
        return;
    }
    presentReleaseNotes(current, {});
}

void Application::presentReleaseNotes(const std::string &version, const std::string &knownNotes)
{
    if (!knownNotes.empty()) {
        m_updates->markVersionSeen();
        m_changelog->present(toQString(version), toQString(knownNotes), toQString(UpdateService::releasePageUrl(version)));
        return;
    }
    QPointer<Application> self(this);
    m_updates->fetchReleaseNotes(version, [self, version](Result<ReleaseNotes> notes) {
        if (!self) {
            return;
        }
        if (notes) {
            self->m_updates->markVersionSeen();
            self->m_changelog->present(toQString(version), toQString(notes.value().notes), toQString(notes.value().page));
            return;
        }
        Logger::info("update", "no what's new for v" + version + ": " + notes.error().userMessage);
        if (notes.error().code == "RELEASE_NOT_FOUND") {
            self->m_updates->markVersionSeen();
            self->m_updates->markChangelogShown();
        }
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
        m_toasts->showTitled(ToastType::Error, trUtf8("Update in Progress"),
                             fromQString(tr("Cannot close while the update is installing (%1%).")
                                             .arg(m_updates->progress())));
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void Application::tell(const Toast &toast)
{
    if (m_settings && m_settings->isVisible()) {
        m_settings->showToast(toast);
    } else {
        m_mainWindow->showToast(toast);
    }
}

void Application::startIssueRecording()
{
    if (!m_recorder || m_recorder->isRecording()) {
        return;
    }
    if (m_reportWindow) {
        m_reportWindow->close();
    }
    if (!m_recordingBar) {
        m_recordingBar = new RecordingBar();
        m_recordingBar->setAttribute(Qt::WA_DeleteOnClose);
        m_recorder->ignore(m_recordingBar);
        connect(m_recordingBar, &RecordingBar::stopRequested, this, &Application::finishIssueRecording);
        connect(m_recordingBar, &RecordingBar::cancelRequested, this, [this] {
            m_recorder->discard();
            m_recordingBar->close();
            tell(Toast{ToastType::Info, {}, trUtf8("Recording cancelled."), kDefaultToastMs});
        });
        connect(m_recorder, &IssueRecorder::tick, m_recordingBar, [this](qint64 elapsed) {
            m_recordingBar->setElapsed(elapsed);
            m_recordingBar->setSteps(m_recorder->eventCount());
        });
    }
    m_recorder->start();
    if (!m_recorder->isRecording()) {
        m_recordingBar->close();
        tell(Toast{ToastType::Error, {}, trUtf8("The recording could not start."), kDefaultToastMs});
        return;
    }
    m_recordingBar->setElapsed(0);
    m_recordingBar->setSteps(0);
    m_recordingBar->placeNear(m_settings && m_settings->isVisible() ? static_cast<QWidget *>(m_settings)
                                                                     : m_mainWindow.get());
    m_recordingBar->show();
    tell(Toast{ToastType::Info, {}, trUtf8("Recording. Show the problem, then press Finish."), kDefaultToastMs});
}

void Application::finishIssueRecording()
{
    m_recorder->stop();
    if (m_recordingBar) {
        m_recordingBar->close();
    }
    m_reportWindow = new IssueReportWindow(m_mainWindow.get());
    m_reportWindow->setSummary(m_recorder->durationMs(), m_recorder->eventCount(), m_recorder->frameCount(),
                               m_recorder->failedImportCount());
    connect(m_reportWindow, &IssueReportWindow::discardRequested, this, [this] { m_recorder->discard(); });
    connect(m_reportWindow, &IssueReportWindow::saveRequested, this,
            [this](const IssueReportWindow::Choices &choices) {
                const QString name = QStringLiteral("SkyAutoPiano-issue-%1%2")
                                         .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")),
                                              QLatin1String(kIssueReportExtension));
                const auto path = chooseReportSavePath(m_reportWindow, name, false);
                if (!path) {
                    return;
                }
                IssueRecorder::SaveOptions options;
                options.description = choices.description;
                options.screenshots = true;
                options.library = choices.library;
                options.failedFiles = choices.failedFiles;
                options.log = true;
                if (auto saved = m_recorder->save(*path, options); !saved) {
                    m_mainWindow->showToast(Toast{ToastType::Error, {}, saved.error().userMessage, kDefaultToastMs});
                    return;
                }
                m_recorder->discard();
                m_reportWindow->close();
                tell(Toast{ToastType::Success, {},
                           trUtf8("Report saved. Send the file to the developer on GitHub or Discord."),
                           kDefaultToastMs});
                openLocalFolder(path->parent_path());
            });
    m_reportWindow->show();
    m_reportWindow->raise();
    m_reportWindow->activateWindow();
}

namespace {

QString crashTitle(const OrderedJson &report)
{
    const OrderedJson crash = report.value("crash", OrderedJson::object());
    const std::string reason = crash.is_object() ? crash.value("reason", std::string()) : std::string();
    const OrderedJson state = report.value("state", OrderedJson::object());
    const bool playing = state.is_object() && state.value("is_playing", false);
    if (reason == "unexpected_exit") {
        return playing ? tr("Closed unexpectedly while playing") : tr("Closed unexpectedly");
    }
    if (reason == "hang") {
        return playing ? tr("Stopped responding while playing") : tr("Stopped responding");
    }
    if (reason == "out_of_memory") {
        return playing ? tr("Ran out of memory while playing") : tr("Ran out of memory");
    }
    return playing ? tr("Crashed while playing") : tr("Crashed");
}

QString durationWords(qint64 milliseconds)
{
    const qint64 minutes = milliseconds / 60000;
    if (minutes >= 60) {
        return tr("%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
    }
    if (minutes >= 1) {
        return tr("%1 min").arg(minutes);
    }
    return tr("%1 s").arg(milliseconds / 1000);
}

bool endedFromOutside(const OrderedJson &report)
{
    const OrderedJson crash = report.value("crash", OrderedJson::object());
    return crash.is_object() && crash.value("reason", std::string()) == "unexpected_exit";
}

std::vector<std::filesystem::path> crashFiles(const std::filesystem::path &directory)
{
    std::vector<std::filesystem::path> files;
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
        if (entry.is_regular_file(error) && entry.path().extension() == kCrashReportExtension) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) { return a.filename() > b.filename(); });
    return files;
}

std::optional<OrderedJson> crashManifest(const std::filesystem::path &file)
{
    const auto text = readArchiveEntry(file, "report.json");
    if (!text) {
        return std::nullopt;
    }
    auto parsed = parseJson<OrderedJson>(*text);
    if (!parsed || !parsed.value().is_object() || !parsed.value().contains("crash")) {
        return std::nullopt;
    }
    return parsed.value();
}

}  // namespace

void Application::refreshCrashList()
{
    if (!m_settings) {
        return;
    }
    std::vector<CrashListEntry> entries;
    for (const auto &file : crashFiles(crashDirectory())) {
        if (entries.size() >= 20) {
            break;
        }
        const auto manifest = crashManifest(file);
        if (!manifest) {
            continue;
        }
        const OrderedJson report = manifest->value("crash", OrderedJson::object());
        if (endedFromOutside(report)) {
            continue;
        }
        const QString id = toQString(file.stem().string());
        QStringList details;
        const QDateTime when = QDateTime::fromString(id.left(15), QStringLiteral("yyyyMMdd-HHmmss"));
        if (when.isValid()) {
            details << QLocale().toString(when, tr("d MMM yyyy, HH:mm"));
        }
        const OrderedJson app = report.value("app", OrderedJson::object());
        if (app.is_object() && app.contains("version")) {
            details << tr("version %1").arg(toQString(app.value("version", std::string())));
        }
        const OrderedJson process = report.value("process", OrderedJson::object());
        if (process.is_object() && process.contains("uptime_ms")) {
            details << tr("ran for %1").arg(durationWords(process.value("uptime_ms", 0LL)));
        }
        if (manifest->contains("dump")) {
            details << tr("crash dump included");
        }
        entries.push_back(CrashListEntry{id, crashTitle(report), details.join(QStringLiteral(" · "))});
    }
    m_settings->setCrashReports(entries);
}

OrderedJson Application::diagnostics() const
{
    OrderedJson result = OrderedJson::object();
    std::size_t customThemes = m_themeLibrary ? m_themeLibrary->themes().size() : 0;
    result["library"] = OrderedJson{{"songs", m_library ? m_library->size() : 0},
                                    {"playlists", m_playlists ? m_playlists->playlists().size() : 0},
                                    {"customThemes", customThemes}};
    OrderedJson activity = OrderedJson::array();
    for (const BreadcrumbEntry &entry : CrashHandler::recentBreadcrumbs()) {
        activity.push_back(OrderedJson{{"t", entry.timeMs}, {"area", entry.category}, {"message", entry.message}});
    }
    result["backgroundActivity"] = std::move(activity);
    OrderedJson crashes = OrderedJson::array();
    for (const auto &file : crashFiles(crashDirectory())) {
        if (crashes.size() >= 10) {
            break;
        }
        const auto manifest = crashManifest(file);
        if (!manifest) {
            continue;
        }
        const OrderedJson report = manifest->value("crash", OrderedJson::object());
        if (endedFromOutside(report)) {
            continue;
        }
        OrderedJson entry{{"id", file.stem().string()}, {"title", fromQString(crashTitle(report))}};
        const OrderedJson app = report.value("app", OrderedJson::object());
        if (app.is_object()) {
            entry["version"] = app.value("version", std::string());
        }
        crashes.push_back(std::move(entry));
    }
    result["recentCrashes"] = std::move(crashes);
    return result;
}

void Application::saveCrashReport(const QString &requested)
{
    std::optional<std::filesystem::path> source;
    if (requested.isEmpty()) {
        source = latestCrashReport(crashDirectory());
    } else {
        const auto file = crashDirectory() / (safeArchiveName(fromQString(requested)) + kCrashReportExtension);
        std::error_code error;
        if (std::filesystem::exists(file, error)) {
            source = file;
        }
    }
    if (!source) {
        tell(Toast{ToastType::Info, {}, trUtf8("That crash report is no longer on this PC."), kDefaultToastMs});
        refreshCrashList();
        return;
    }
    QWidget *parent = m_settings ? static_cast<QWidget *>(m_settings) : m_mainWindow.get();
    const std::string id = source->stem().string();
    const auto path = chooseReportSavePath(
        parent, QStringLiteral("SkyAutoPiano-crash-%1%2").arg(toQString(id), QLatin1String(kCrashReportExtension)), true);
    if (!path) {
        return;
    }
    std::error_code error;
    std::filesystem::copy_file(*source, *path, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        tell(Toast{ToastType::Error, {}, trUtf8("The crash report could not be saved."), kDefaultToastMs});
        return;
    }
    tell(Toast{ToastType::Success, {},
               trUtf8("Crash report saved. Send the file to the developer on GitHub or Discord."), kDefaultToastMs});
    openLocalFolder(path->parent_path());
}

void Application::applyStartupLayout()
{
    const QStringList arguments = QApplication::arguments();
    const qsizetype at = arguments.indexOf(QStringLiteral("--layout"));
    if (at < 0 || at + 1 >= arguments.size()) {
        return;
    }
    auto text = readTextFile(std::filesystem::path(arguments[at + 1].toStdWString()));
    if (!text) {
        return;
    }
    auto parsed = parseJson<OrderedJson>(text.value());
    if (!parsed || !parsed.value().is_object()) {
        return;
    }
    const OrderedJson &layout = parsed.value();
    const auto apply = [](QWidget *window, const OrderedJson &entry) {
        const int width = entry.value("width", 0);
        const int height = entry.value("height", 0);
        if (width > 0 && height > 0) {
            window->resize(width, height);
        }
        if (entry.value("state", std::string()) == "maximized") {
            window->showMaximized();
        }
    };
    if (const auto main = layout.find("main"); main != layout.end() && main->is_object()) {
        apply(m_mainWindow.get(), *main);
    }
    if (const auto settings = layout.find("settings"); settings != layout.end() && settings->is_object()) {
        openSettings();
        if (m_settings) {
            apply(m_settings, *settings);
            if (const auto page = settings->find("page"); page != settings->end() && page->is_number_integer()) {
                m_settings->openPage(page->get<int>());
            }
        }
    }
}

void Application::notifyLegacy(bool manual, const Toast &toast)
{
    if (manual && m_settings) {
        m_settings->showToast(toast);
    } else {
        m_mainWindow->showToast(toast);
    }
}

std::map<std::string, std::string> Application::legacyExistingPayloads() const
{
    std::map<std::string, std::string> payloads;
    for (const SheetEntry &entry : m_library->entries()) {
        payloads.emplace(legacySongKey(entry.json()), entry.payloadFile());
    }
    return payloads;
}

void Application::detectLegacyLibrary(bool manual)
{
    if (m_legacyBusy) {
        return;
    }
    m_legacyBusy = true;
    const std::set<std::string> keys = m_library->legacySongKeys();
    const std::map<std::string, std::string> existingPayloads = legacyExistingPayloads();
    const std::filesystem::path own = Paths::dataDirectory();
    QPointer<Application> self(this);
    QThreadPool::globalInstance()->start([self, keys, existingPayloads, own, manual] {
        std::optional<LegacyImportPlan> best;
        for (const LegacyInstall &install : findLegacyInstalls(own, defaultLegacySearchRoots())) {
            auto plan = planLegacyImport(install, keys, electronLocalStorageDirectory(), own, existingPayloads);
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
            notifyLegacy(true, Toast{ToastType::Warning, {}, trUtf8("No previous version of Sky Auto Piano was found."),
                                     kDefaultToastMs});
            pickLegacyFolder();
        }
        return;
    }
    if (plan->rows.empty() && plan->extras.empty()) {
        if (manual) {
            notifyLegacy(true, Toast{ToastType::Info, {},
                                     trUtf8("All songs from the previous version are already in your library."),
                                     kDefaultToastMs});
        } else {
            m_uiState->setLegacyImportOffered();
        }
        return;
    }
    m_uiState->setLegacyMigration("available", legacySourceFingerprint(plan->install), {});
    if (manual) {
        importLegacy(*plan, true);
        return;
    }
    m_uiState->setLegacyImportOffered();
    const LegacyImportPlan accepted = *plan;
    const auto songs = accepted.rows.size();
    const auto playlists = accepted.extras.playlists.size();
    m_mainWindow->confirm(
        tr("Found a previous version of Sky Auto Piano with %1 and %2. Import your songs, playlists, favorites and "
           "settings into this version?")
            .arg(songCount(songs), playlistCount(playlists)),
        [this, accepted] { importLegacy(accepted, false); });
}

void Application::pickLegacyFolder()
{
    QWidget *parent = m_settings ? static_cast<QWidget *>(m_settings) : m_mainWindow.get();
    const auto folder = chooseFolder(parent, tr("Select the previous Sky Auto Piano folder"));
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
    const std::map<std::string, std::string> existingPayloads = legacyExistingPayloads();
    const std::filesystem::path own = Paths::dataDirectory();
    QPointer<Application> self(this);
    QThreadPool::globalInstance()->start([self, keys, existingPayloads, own, folder = *folder] {
        std::optional<LegacyImportPlan> plan;
        if (const auto install = inspectLegacyFolder(folder)) {
            if (auto planned = planLegacyImport(*install, keys, electronLocalStorageDirectory(), own, existingPayloads)) {
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
                            tr("That folder does not contain songs from Sky Auto Piano. Try another folder."));
                        return;
                    }
                    self->notifyLegacy(true, Toast{ToastType::Error, {},
                                                   trUtf8("That folder does not contain songs from Sky Auto Piano."),
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
    const std::string fingerprint = legacySourceFingerprint(plan.install);
    m_uiState->setLegacyMigration("importing", fingerprint, {});
    // Snapshot the pre-existing destination files before anything mutates
    // them, so a partial merge stays recoverable. The legacy source itself
    // is only ever read.
    (void)snapshotDestinationFiles(Paths::dataDirectory(), Paths::configDirectory(), fingerprint);
    QPointer<Application> self(this);
    m_library->importLegacyRows(plan.install.dataDirectory, plan.rows, plan.alreadyPresent,
                                [self, manual, plan, fingerprint](LegacyImportSummary summary) {
                                    if (!self) {
                                        return;
                                    }
                                    self->m_legacyBusy = false;
                                    const LegacyExtrasOutcome outcome = self->applyLegacyExtras(plan, summary);

                                    LegacyImportResult result;
                                    result.sourceFingerprint = fingerprint;
                                    result.sheets.planned = static_cast<int>(plan.rows.size()) +
                                                            summary.alreadyPresent +
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
                                    const LegacyVerification payloadsCheck = verifyImportedSheets(
                                        plan.install.dataDirectory, Paths::dataDirectory(), expected, {});
                                    const LegacyVerification referencesCheck = verifyImportedSheets(
                                        plan.install.dataDirectory, Paths::dataDirectory(), {}, referenced);
                                    result.sheets.validated =
                                        std::max(0, static_cast<int>(expected.size()) - payloadsCheck.failed);
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
                                    const bool verified = payloadsCheck.verified && referencesCheck.verified &&
                                                          summary.failed == 0 && plan.missingPayloads == 0 &&
                                                          plan.conflicts.empty() &&
                                                          outcome.playlistMissingReferences == 0 &&
                                                          !outcome.settingsRejected;
                                    result.verificationStatus = verified ? "verified" : "needs_review";
                                    self->m_uiState->setLegacyMigration(result.verificationStatus, fingerprint,
                                                                        result.toJson());
                                    Logger::info("app", "legacy import result status=" + result.verificationStatus +
                                                            " copied=" + std::to_string(result.sheets.copied) +
                                                            " conflicts=" + std::to_string(result.sheets.conflicts) +
                                                            " failed=" + std::to_string(result.sheets.failed) +
                                                            " validated=" + std::to_string(result.sheets.validated));

                                    QString message = tr("Imported %1 from the previous version.").arg(toQString(outcome.text));
                                    if (summary.failed > 0) {
                                        message += QLatin1Char(' ') + (summary.failed == 1
                                                                           ? tr("1 song could not be copied.")
                                                                           : tr("%1 songs could not be copied.")
                                                                                 .arg(summary.failed));
                                    }
                                    if (!plan.conflicts.empty()) {
                                        message += QLatin1Char(' ') +
                                                   tr("%1 song(s) share a name with a different song already in "
                                                      "your library and were skipped.")
                                                       .arg(static_cast<int>(plan.conflicts.size()));
                                    }
                                    if (!verified) {
                                        message += QLatin1Char(' ') +
                                                   tr("Some items need attention - you can retry the import from Settings.");
                                    }
                                    const std::string text = fromQString(message);
                                    self->notifyLegacy(manual, Toast{verified ? ToastType::Success : ToastType::Warning,
                                                                     {}, text, kDefaultToastMs});
                                    if (self->m_quickStart &&
                                        self->m_quickStart->legacyState() == QuickStart::LegacyState::Importing) {
                                        self->m_quickStart->setLegacyState(QuickStart::LegacyState::Imported, 0, 0,
                                                                           toQString(text));
                                    }
                                    if (self->m_settings) {
                                        self->m_settings->reloadForm();
                                    }
                                    self->refreshLegacyCleanup();
                                });
}

Application::LegacyExtrasOutcome Application::applyLegacyExtras(const LegacyImportPlan &plan,
                                                                const LegacyImportSummary &summary)
{
    LegacyExtrasOutcome outcome;
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
        int legacyKeys = 0;
        if (legacy.contains("sheetKeys") && legacy["sheetKeys"].is_array()) {
            for (const OrderedJson &key : legacy["sheetKeys"]) {
                if (key.is_string()) {
                    ++legacyKeys;
                    if (const auto mapped = payloadFor.find(key.get<std::string>()); mapped != payloadFor.end()) {
                        keys.push_back(mapped->second);
                    }
                }
            }
        }
        outcome.playlistReferencesRemapped += static_cast<int>(keys.size());
        outcome.playlistMissingReferences += legacyKeys - static_cast<int>(keys.size());
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
            ++outcome.playlistsCreated;
        } else {
            ++outcome.playlistsMerged;
        }
        if (!keys.empty()) {
            (void)m_playlists->addSheets(id, keys);
        }
        ++playlists;
    }

    for (const std::string &name : extras.favorites) {
        if (!m_favorites->isFavorite(name)) {
            m_favorites->toggle(name);
            ++outcome.favoritesImported;
        } else {
            ++outcome.favoritesAlreadyPresent;
        }
    }
    if (!extras.recentPlays.empty()) {
        if (m_recent->names().empty()) {
            for (auto it = extras.recentPlays.rbegin(); it != extras.recentPlays.rend(); ++it) {
                m_recent->add(*it);
            }
            outcome.recentImported = true;
        } else {
            outcome.recentSkipped = true;
        }
    }

    if (extras.config) {
        outcome.settingsRejected = true;
        if (auto merged = ConfigService::normalizeImported(m_config->raw(), *extras.config)) {
            if (ConfigService::validateShortcuts(ConfigService::fromJson(merged.value()).shortcut) &&
                m_config->replace(merged.value())) {
                outcome.settingsApplied = true;
                outcome.settingsRejected = false;
                m_shortcuts->registerAll();
            }
        }
    }
    if (extras.theme) {
        if (!m_themes->followsSystem()) {
            applyTheme(*extras.theme == "dark" ? AppTheme::Dark : AppTheme::Light);
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
        parts << (outcome.favoritesImported == 1 ? tr("1 favorite")
                                                 : tr("%1 favorites").arg(outcome.favoritesImported));
    }
    if (outcome.settingsApplied) {
        parts << tr("your settings");
    }
    outcome.text = fromQString(joinList(parts));
    return outcome;
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
        m_editor = new EditorWindow(*m_library, *m_uiState, Paths::dataDirectory() / "soundpack" / "Piano",
                                    m_mainWindow.get());
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
        refreshLegacyCleanup();
        return;
    }
    m_toasts->show(ToastType::Warning, trUtf8(kSettingsWarning));
    m_shortcuts->suspend();
    m_settings = new SettingsWindow(SettingsServices{*m_config, *m_updates, *m_themes}, m_mainWindow.get());
    connect(m_settings, &QObject::destroyed, this, [this] { m_shortcuts->resume(); });
    connect(m_settings, &SettingsWindow::themeImported, this, &Application::applyTheme);
    connect(m_settings, &SettingsWindow::importPreviousRequested, this, [this] { detectLegacyLibrary(true); });
    connect(m_settings, &SettingsWindow::legacyRemovalRequested, this, &Application::onLegacyRemovalClicked);
    connect(m_settings, &SettingsWindow::legacyVerifyRequested, this, &Application::onLegacyVerifyClicked);
    connect(m_settings, &SettingsWindow::recordIssueRequested, this, &Application::startIssueRecording);
    connect(m_settings, &SettingsWindow::saveCrashReportRequested, this, &Application::saveCrashReport);
    connect(m_settings, &SettingsWindow::clearCrashReportsRequested, this, [this] {
        const std::size_t removed = deleteCrashReports(crashDirectory());
        refreshCrashList();
        if (m_settings) {
            m_settings->showToast(Toast{ToastType::Success, {},
                                        removed == 1 ? trUtf8("Deleted 1 crash report.")
                                                     : fromQString(tr("Deleted %1 crash reports.").arg(removed)),
                                        kDefaultToastMs});
        }
    });
    connect(m_settings, &SettingsWindow::crashFolderRequested, this, [this] {
        std::error_code error;
        std::filesystem::create_directories(crashDirectory(), error);
        if (!openLocalFolder(crashDirectory())) {
            tell(Toast{ToastType::Warning, {}, trUtf8("The crash folder could not be opened."), kDefaultToastMs});
        }
    });
    refreshCrashList();
    refreshLegacyCleanup();
    m_settings->setLanguage(toQString(m_uiState->language()));
    connect(m_settings, &SettingsWindow::languageChosen, this,
            [this](const QString &preference) { m_uiState->setLanguage(fromQString(preference)); });
    connect(m_settings, &SettingsWindow::restartRequested, this, &Application::requestRelaunch);
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
        m_toasts->show(ToastType::Error,
                       trUtf8("Sky Auto Piano closed unexpectedly last time. Save the crash report in Settings → Reports to send "
                       "it to the developer."),
                       kDefaultToastMs);
    }
    applyStartupLayout();
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
        connect(m_quickStart, &QuickStart::legacyRemovalRequested, this, &Application::onLegacyRemovalClicked);
        connect(m_quickStart, &QuickStart::legacyVerifyRequested, this, &Application::onLegacyVerifyClicked);
    }
    if (!m_quickStart->isOpen()) {
        m_quickStart->start();
        m_quickStartLegacy.reset();
        m_legacyForQuickStart = true;
        detectLegacyLibrary(false);
    }
    refreshLegacyCleanup();
}

LegacyImportEvidence Application::legacyRemovalEvidence(const LegacyInstallInfo &install) const
{
    LegacyImportEvidence evidence;
    evidence.migrationStatus = m_uiState->legacyMigrationStatus();
    evidence.sourceFingerprint = m_uiState->legacyMigration().value("sourceFingerprint", std::string());
    evidence.legacyRemoved = m_uiState->legacyRemoved();
    const OrderedJson result = m_uiState->legacyMigration().value("result", OrderedJson::object());
    const OrderedJson sheets = result.value("sheets", OrderedJson::object());
    const OrderedJson playlists = result.value("playlists", OrderedJson::object());
    const OrderedJson config = result.value("config", OrderedJson::object());
    const OrderedJson verification = result.value("verification", OrderedJson::object());
    evidence.hasSeriousFailures = sheets.value("failed", 0) > 0 || sheets.value("conflicts", 0) > 0 ||
                                  playlists.value("missingReferences", 0) > 0 || config.value("rejected", false) ||
                                  verification.value("failed", 0) > 0;
    // A reconciliation result earned earlier in this session stays valid for
    // the same source; a different source always has to earn its own.
    if (m_removalEvidence && m_removalEvidence->sourceFingerprint == install.sourceFingerprint) {
        evidence.reVerified = m_removalEvidence->reVerified;
        evidence.reVerificationFailed = m_removalEvidence->reVerificationFailed;
    }
    return evidence;
}

void Application::refreshLegacyCleanup()
{
    if (!m_removal) {
        return;
    }
    m_removalDetection = m_removal->detectLegacyInstall();
    if (m_removalDetection->install) {
        m_removalEvidence = legacyRemovalEvidence(*m_removalDetection->install);
    } else {
        m_removalEvidence.reset();
    }
    updateLegacyCleanupUi();
}

void Application::updateLegacyCleanupUi()
{
    if (!m_removalDetection) {
        return;
    }
    const LegacyDetection &detection = *m_removalDetection;
    const bool uninstalling = m_removal->removalInProgress();
    const bool busy = uninstalling || m_removalVerifying;
    QString description;
    bool removeEnabled = false;
    bool verifyVisible = false;
    if (m_removalVerifying) {
        description = tr("Verifying that everything from the old version is complete in this library...");
    } else if (detection.status == LegacyDetectionStatus::NotFound) {
        description = tr("No old Electron version was found on this PC.");
    } else if (detection.status == LegacyDetectionStatus::Ambiguous) {
        description = tr("More than one old installation is registered, so automatic removal is off. You can "
                         "remove the old version in Windows Settings > Apps.");
    } else if (detection.status == LegacyDetectionStatus::Invalid) {
        description = tr("The old version's uninstaller could not be validated, so automatic removal is off. "
                         "You can remove the old version in Windows Settings > Apps.");
    } else if (uninstalling) {
        description = tr("Uninstalling the old Electron version...");
    } else if (m_removalEvidence) {
        const RemovalGate gate = m_removal->canOfferRemoval(*m_removalEvidence, detection);
        removeEnabled = gate.allowed;
        if (gate.allowed) {
            description = tr("Your data from the old version is verified in this library. This library is "
                             "independent and does not sync back to the old version.");
        } else {
            switch (gate.blockers.front()) {
                case RemovalBlocker::AlreadyRemoved:
                    description = tr("The old Electron version has already been removed.");
                    break;
                case RemovalBlocker::ImportNotVerified:
                    description = tr("Removal unlocks after your data from the old version has been imported "
                                     "and verified.");
                    break;
                case RemovalBlocker::SourceMismatch:
                    description = tr("The verified import came from a different copy of the old version, so "
                                     "removal is off.");
                    break;
                case RemovalBlocker::SeriousImportIssues:
                    description = tr("The last import had problems that need review, so removal is off.");
                    break;
                case RemovalBlocker::ReverificationRequired:
                    description = tr("Your imported data needs a fresh verification before removal.");
                    break;
                case RemovalBlocker::PathOverlap:
                    description = tr("The old and new versions share folders, so removal is off for safety.");
                    break;
                case RemovalBlocker::LegacyRunning:
                    description = tr("The old Electron version is still running. Close it, then try again.");
                    break;
                default:
                    description = tr("The old version could not be validated for removal.");
                    break;
            }
            verifyVisible =
                m_removalEvidence->migrationStatus == "verified" &&
                std::find(gate.blockers.begin(), gate.blockers.end(), RemovalBlocker::ReverificationRequired) !=
                    gate.blockers.end();
        }
    }
    if (m_settings) {
        m_settings->setLegacyCleanup(description, removeEnabled, verifyVisible, busy);
    }
    if (m_quickStart) {
        m_quickStart->setLegacyCleanup(description, removeEnabled, verifyVisible, busy);
    }
}

void Application::onLegacyVerifyClicked()
{
    if (!m_removalDetection || !m_removalDetection->install || m_removalVerifying || m_legacyBusy) {
        return;
    }
    const LegacyInstallInfo install = *m_removalDetection->install;
    const std::set<std::string> keys = m_library->legacySongKeys();
    const std::map<std::string, std::string> existingPayloads = legacyExistingPayloads();
    const std::filesystem::path own = Paths::dataDirectory();
    const std::filesystem::path localStorage = electronLocalStorageDirectory();
    m_removalVerifying = true;
    updateLegacyCleanupUi();
    QPointer<Application> self(this);
    QThreadPool::globalInstance()->start([self, install, keys, existingPayloads, own, localStorage] {
        bool verified = false;
        int failed = 0;
        auto plan = planLegacyImport(install.legacy, keys, localStorage, own, existingPayloads);
        if (plan && plan.value().conflicts.empty() && plan.value().missingPayloads == 0 &&
            plan.value().rows.empty()) {
            std::vector<std::pair<std::string, std::string>> expected;
            std::vector<std::string> referenced;
            bool complete = true;
            for (const OrderedJson &row : plan.value().allRows) {
                const auto found = existingPayloads.find(legacySongKey(row));
                if (found == existingPayloads.end()) {
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
                if (!self) {
                    return;
                }
                self->m_removalVerifying = false;
                if (self->m_removalEvidence) {
                    self->m_removalEvidence->reVerified = verified;
                    self->m_removalEvidence->reVerificationFailed = failed;
                }
                self->tell(Toast{verified ? ToastType::Success : ToastType::Warning, {},
                                 verified ? trUtf8("Your data from the old version is verified in this library.")
                                          : trUtf8("Some data from the old version could not be verified. "
                                                   "Import it again from Settings."),
                                 kDefaultToastMs});
                self->updateLegacyCleanupUi();
            },
            Qt::QueuedConnection);
    });
}

void Application::onLegacyRemovalClicked()
{
    refreshLegacyCleanup();
    if (!m_removalDetection || !m_removalDetection->install || !m_removalEvidence) {
        return;
    }
    const RemovalGate gate = m_removal->canOfferRemoval(*m_removalEvidence, *m_removalDetection);
    if (!gate.allowed) {
        tell(Toast{ToastType::Warning, {},
                   trUtf8("The old version cannot be removed yet. See the explanation in Settings."),
                   kDefaultToastMs});
        return;
    }
    QWidget *window = m_quickStart && m_quickStart->isOpen()
                          ? static_cast<QWidget *>(m_mainWindow.get())
                          : (m_settings ? static_cast<QWidget *>(m_settings)
                                        : static_cast<QWidget *>(m_mainWindow.get()));
    if (m_removalDialog && m_removalDialog->parentWidget() != window) {
        m_removalDialog->deleteLater();
        m_removalDialog = nullptr;
    }
    if (!m_removalDialog) {
        m_removalDialog = new ConfirmDialog(
            window,
            {QStringLiteral("trash3"), tr("Remove the old Electron version?"),
             tr("The old version's own uninstaller will open and remove it. Your songs and settings in this "
                "version stay untouched - this library is independent and does not sync back. Leftover data "
                "files of the old version are not deleted, but its .myp file association will go away."),
             tr("Maybe later"), QString(), tr("Remove old version"), QStringLiteral("trash3"),
             ActionButton::Kind::Amber});
    }
    QPointer<Application> self(this);
    m_removalDialog->ask([self](ConfirmDialog::Choice choice) {
        if (self && choice == ConfirmDialog::Choice::Primary) {
            self->startLegacyRemoval();
        }
    });
}

void Application::startLegacyRemoval()
{
    if (!m_removalDetection || !m_removalDetection->install || !m_removalEvidence) {
        return;
    }
    const RemovalOutcome outcome = m_removal->requestLegacyRemoval(*m_removalDetection->install, *m_removalEvidence);
    if (outcome.status == RemovalStatus::Launched) {
        tell(Toast{ToastType::Info, {},
                   trUtf8("The old version's uninstaller is open. Follow its steps to finish."), kDefaultToastMs});
        m_removalTimer->start();
    } else if (outcome.status == RemovalStatus::LaunchFailed) {
        tell(Toast{ToastType::Error, {}, trUtf8("The old version's uninstaller could not be started."),
                   kDefaultToastMs});
    } else if (outcome.status == RemovalStatus::Blocked) {
        tell(Toast{ToastType::Warning, {},
                   trUtf8("The old version cannot be removed yet. See the explanation in Settings."),
                   kDefaultToastMs});
    }
    refreshLegacyCleanup();
}

void Application::pollLegacyRemoval()
{
    const std::optional<RemovalOutcome> outcome = m_removal->pollRemoval();
    if (!outcome) {
        return;
    }
    m_removalTimer->stop();
    if (outcome->status == RemovalStatus::Completed) {
        m_uiState->setLegacyRemoved();
        tell(Toast{ToastType::Success, {},
                   trUtf8("The old Electron version was removed. Your library here is unchanged."),
                   kDefaultToastMs});
    } else if (outcome->status == RemovalStatus::Incomplete) {
        tell(Toast{ToastType::Warning, {},
                   trUtf8("The old version is still installed. You can try removing it again."), kDefaultToastMs});
    } else {
        tell(Toast{ToastType::Error, {},
                   trUtf8("The old version was removed, but this library did not pass its check afterwards."),
                   kDefaultToastMs});
    }
    refreshLegacyCleanup();
}

void Application::activate()
{
    m_mainWindow->bringToFront();
}

}  // namespace sky
