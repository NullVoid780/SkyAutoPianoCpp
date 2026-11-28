#include "ui/settings_window/settings_window.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QThreadPool>
#include <QUrl>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <algorithm>

#include "core/translator.hpp"
#include "app/paths.hpp"
#include "app/version.hpp"
#include "core/string_utils.hpp"
#include "platform/file_dialog.hpp"
#include "platform/shell_open.hpp"
#include "platform/window_attributes.hpp"
#include "services/settings/config_service.hpp"
#include "services/update/update_service.hpp"
#include "ui/core/easing.hpp"
#include "ui/core/elevation_layer.hpp"
#include "ui/core/smooth_scroll.hpp"
#include "ui/main_window/changelog_panel.hpp"
#include "ui/settings_window/confirm_dialog.hpp"
#include "ui/settings_window/general_tab.hpp"
#include "ui/settings_window/keyboard_tab.hpp"
#include "ui/settings_window/save_button.hpp"
#include "ui/settings_window/settings_sidebar.hpp"
#include "ui/settings_window/shortcuts_tab.hpp"
#include "ui/settings_window/themes_tab.hpp"
#include "ui/settings_window/updates_tab.hpp"
#include "ui/settings_window/about_tab.hpp"
#include "ui/settings_window/reports_tab.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/toast_host.hpp"

namespace sky {

namespace {

constexpr int kWidth = 980;
constexpr int kHeight = 740;
constexpr int kMinWidth = 760;
constexpr int kMinHeight = 560;
constexpr int kContentMaxWidth = 960;
constexpr int kContentPadding = 20;
constexpr int kContentBottom = 100;
constexpr int kPageRise = 10;
constexpr int kSaveInset = 20;
constexpr const char *kCommunityUrl = "https://discord.gg/Mu8RS9FZky";
constexpr const char *kOriginalProjectUrl = "https://github.com/HerokeyVN/Sky-Auto-Piano";
constexpr const char *kSourceCodeUrl = "https://github.com/NullVoid780/SkyAutoPianoCpp";

template <typename Work, typename Done>
void runInBackground(QObject *context, Work work, Done done)
{
    QPointer<QObject> guard(context);
    QThreadPool::globalInstance()->start([guard, work = std::move(work), done = std::move(done)]() mutable {
        auto result = work();
        QMetaObject::invokeMethod(
            qApp,
            [guard, done = std::move(done), result = std::move(result)]() mutable {
                if (guard) {
                    done(std::move(result));
                }
            },
            Qt::QueuedConnection);
    });
}

const char *playbackModeName(PlaybackMode mode)
{
    return mode == PlaybackMode::Strict ? "strict" : "lite";
}

}  // namespace

SettingsWindow::SettingsWindow(SettingsServices services, QWidget *owner)
    : QWidget(owner, Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint |
                         Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint),
      m_services(services),
      m_sidebar(new SettingsSidebar(QLatin1String(kAppVersion), this)),
      m_scroll(new QScrollArea(this)),
      m_pageHost(new QWidget()),
      m_pageLayout(new QVBoxLayout(m_pageHost)),
      m_general(new GeneralTab(m_pageHost)),
      m_keyboard(new KeyboardTab(m_pageHost)),
      m_shortcuts(new ShortcutsTab(m_pageHost)),
      m_themes(new ThemesTab(services.themes, m_pageHost)),
      m_updates(new UpdatesTab(QLatin1String(kAppVersion), m_pageHost)),
      m_reports(new ReportsTab(m_pageHost)),
      m_about(new AboutTab(QLatin1String(kAppVersion), m_pageHost)),
      m_save(new SaveButton(this)),
      m_shadows(new ElevationLayer(this)),
      m_blockDialog(new ConfirmDialog(
          this, {QStringLiteral("exclamation-triangle"), tr("Block Automatic Updates?"),
                 tr("Are you sure you want to block automatic updates?\n"
                                "You can still check for updates manually anytime."),
                 tr("Cancel"), QString(), tr("Yes, Block Updates"),
                 QStringLiteral("shield-check"), ActionButton::Kind::Amber})),
      m_unsavedDialog(new ConfirmDialog(
          this, {QStringLiteral("save"), tr("Save your changes?"),
                 tr("You changed some settings but didn't save them."), tr("Cancel"),
                 QString(), tr("Save"), QStringLiteral("save"), ActionButton::Kind::Primary})),
      m_clearCrashesDialog(new ConfirmDialog(
          this, {QStringLiteral("trash3"), tr("Delete all crash reports?"),
                 tr("They can't be brought back. Save any report you still want to send first."),
                 tr("Cancel"), QString(), tr("Delete all"), QStringLiteral("trash3"),
                 ActionButton::Kind::Amber})),
      m_whatsNew(new ChangelogPanel(this)),
      m_toasts(new ToastHost(this)),
      m_pageFade(new QVariantAnimation(this))
{
    setObjectName(QStringLiteral("SettingsWindow"));
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Settings"));
    setWindowIcon(QIcon(QStringLiteral(":/icon/Icon9.ico")));
    resize(kWidth, kHeight);
    setMinimumSize(kMinWidth, kMinHeight);
    setFocusPolicy(Qt::ClickFocus);

    m_pageHost->setObjectName(QStringLiteral("SettingsPageHost"));
    applyPageMargins(0);
    m_pageLayout->setSpacing(0);
    m_pages = {m_general, m_keyboard, m_shortcuts, m_themes, m_updates, m_reports, m_about};
    for (QWidget *page : m_pages) {
        m_pageLayout->addWidget(page);
        page->hide();
    }
    m_scroll->setObjectName(QStringLiteral("SettingsContent"));
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setWidgetResizable(true);
    m_scroll->setWidget(m_pageHost);
    m_scroll->viewport()->setObjectName(QStringLiteral("SettingsContentViewport"));
    m_scroll->verticalScrollBar()->setSingleStep(40);
    m_smoothScroll = new SmoothScroll(m_scroll);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(m_sidebar);
    row->addWidget(m_scroll, 1);

    m_generalNav = m_sidebar->addItem(QStringLiteral("sliders"), tr("General"));
    m_sidebar->addItem(QStringLiteral("keyboard"), tr("Keyboard"));
    m_sidebar->addItem(QStringLiteral("lightning"), tr("Shortcuts"));
    m_sidebar->addItem(QStringLiteral("palette"), tr("Themes"));
    m_updatesNav = m_sidebar->addItem(QStringLiteral("update"), tr("Updates"));
    m_sidebar->addItem(QStringLiteral("bug"), tr("Reports"));
    m_sidebar->addSeparator();
    m_sidebar->addItem(QStringLiteral("info-circle"), tr("About"));

    m_shadows->setFloating(true);
    m_shadows->track(m_sidebar, 0, [] { return std::vector<ShadowSpec>{currentTheme().settings.sidebarShadow}; });

    m_pageFade->setDuration(kHoverSlowMs);
    m_pageFade->setEasingCurve(easingCurve(Easing::Ease));
    connect(m_pageFade, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        const qreal progress = value.toReal();
        applyPageMargins(qRound(kPageRise * (1.0 - progress)));
        if (const int index = m_sidebar->current(); index >= 0) {
            if (auto *effect = qobject_cast<QGraphicsOpacityEffect *>(m_pages[static_cast<std::size_t>(index)]->graphicsEffect())) {
                effect->setOpacity(progress);
            }
        }
    });
    connect(m_pageFade, &QVariantAnimation::finished, this, [this] {
        for (QWidget *page : m_pages) {
            page->setGraphicsEffect(nullptr);
        }
    });

    connect(m_sidebar, &SettingsSidebar::currentChanged, this, &SettingsWindow::showPage);
    connect(m_save, &QAbstractButton::clicked, this, &SettingsWindow::save);
    connect(m_general, &GeneralTab::exportRequested, this, &SettingsWindow::exportSettings);
    connect(m_general, &GeneralTab::importRequested, this, &SettingsWindow::importSettings);
    connect(m_general, &GeneralTab::legacyRemovalRequested, this, &SettingsWindow::legacyRemovalRequested);
    connect(m_general, &GeneralTab::quickStartRequested, this, &SettingsWindow::quickStartRequested);
    connect(m_general, &GeneralTab::languageChosen, this, &SettingsWindow::languageChosen);
    connect(m_general, &GeneralTab::restartRequested, this, &SettingsWindow::restartRequested);
    connect(m_reports, &ReportsTab::recordIssueRequested, this, &SettingsWindow::recordIssueRequested);
    connect(m_reports, &ReportsTab::saveCrashReportRequested, this, &SettingsWindow::saveCrashReportRequested);
    connect(m_reports, &ReportsTab::crashFolderRequested, this, &SettingsWindow::crashFolderRequested);
    connect(m_reports, &ReportsTab::clearCrashReportsRequested, this, [this] {
        QPointer<SettingsWindow> self(this);
        m_clearCrashesDialog->ask([self](ConfirmDialog::Choice choice) {
            if (self && choice == ConfirmDialog::Choice::Primary) {
                emit self->clearCrashReportsRequested();
            }
        });
    });
    connect(m_themes, &ThemesTab::toastRequested, this, &SettingsWindow::showToast);
    connect(m_themes, &ThemesTab::editingChanged, this, [this] {
        m_smoothScroll->stop();
        m_scroll->verticalScrollBar()->setValue(0);
    });
    connect(m_updates, &UpdatesTab::checkRequested, this, &SettingsWindow::checkForUpdates);
    connect(m_updates, &UpdatesTab::whatsNewRequested, this, &SettingsWindow::showWhatsNew);
    if (const auto &last = m_services.updates.lastCheck()) {
        showUpdateResult(last->available ? UpdateOverviewCard::State::Available : UpdateOverviewCard::State::UpToDate,
                              toQString(last->latestVersion));
    }
    connect(m_updates, &UpdatesTab::blockUpdatesToggled, this, &SettingsWindow::onBlockUpdatesToggled);
    connect(m_about, &AboutTab::originalProjectRequested, this,
            [] { openExternalUrl(QUrl(QString::fromLatin1(kOriginalProjectUrl))); });
    connect(m_about, &AboutTab::sourceCodeRequested, this,
            [] { openExternalUrl(QUrl(QString::fromLatin1(kSourceCodeUrl))); });
    connect(m_about, &AboutTab::dataFolderRequested, this, [this] {
        if (!openLocalFolder(Paths::dataDirectory())) {
            showToast(Toast{ToastType::Warning, {}, trUtf8("The data folder could not be opened."), kDefaultToastMs});
        }
    });
    connect(m_about, &AboutTab::licensesRequested, this, [this] {
        if (!openLocalFolder(Paths::appDirectory() / "licenses")) {
            showToast(Toast{ToastType::Warning, {}, trUtf8("The licenses folder is only included in the installed app."),
                            kDefaultToastMs});
        }
    });
    connect(m_updates, &UpdatesTab::communityRequested, this,
            [] { openExternalUrl(QUrl(QString::fromLatin1(kCommunityUrl))); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        applyThemeTitleBar(this);
        update();
    });

    loadForm(m_services.config.value());
    m_sidebar->setCurrent(0);
    applyThemeTitleBar(this);
    positionOverlays();
}

void SettingsWindow::showPage(int index)
{
    for (std::size_t i = 0; i < m_pages.size(); ++i) {
        QWidget *page = m_pages[i];
        const bool active = static_cast<int>(i) == index;
        if (active) {
            auto *effect = new QGraphicsOpacityEffect(page);
            effect->setOpacity(0.0);
            page->setGraphicsEffect(effect);
        }
        page->setVisible(active);
    }
    QWidget *shown = m_pages[static_cast<std::size_t>(index)];
    m_save->setVisible(shown != m_themes && shown != m_reports);
    m_smoothScroll->stop();
    m_scroll->verticalScrollBar()->setValue(0);
    m_pageFade->stop();
    m_pageFade->setStartValue(0.0);
    m_pageFade->setEndValue(1.0);
    m_pageFade->start();
    if (index == m_generalNav) {
        emit legacyCleanupShown();
    }
}

void SettingsWindow::loadForm(const Config &config)
{
    m_general->load(config);
    m_keyboard->load(config);
    m_shortcuts->load(config);
    m_updates->load(config);
}

Config SettingsWindow::formConfig() const
{
    Config config = m_services.config.value();
    m_general->collect(config);
    m_keyboard->collect(config);
    m_shortcuts->collect(config);
    m_updates->collect(config);
    return config;
}

OrderedJson SettingsWindow::mergedJson(const Config &form) const
{
    OrderedJson next = m_services.config.raw();
    if (!next.is_object()) {
        next = ConfigService::defaults();
    }
    for (const char *section : {"panel", "keyboard", "shortcut", "update"}) {
        if (!next[section].is_object()) {
            next[section] = OrderedJson::object();
        }
    }
    next["panel"]["autoSave"] = form.panel.autoSave;
    next["panel"]["minimizeOnPlay"] = form.panel.minimizeOnPlay;
    next["panel"]["pauseWhenUnfocused"] = form.panel.pauseWhenUnfocused;
    next["panel"]["playbackMode"] = playbackModeName(form.panel.playbackMode);
    OrderedJson keys = OrderedJson::array();
    for (const std::string &key : form.keyboard.keys) {
        keys.push_back(key);
    }
    next["keyboard"]["keys"] = std::move(keys);
    next["keyboard"]["customKeyboard"] = form.keyboard.customKeyboard;
    next["shortcut"]["pre"] = form.shortcut.pre;
    next["shortcut"]["play"] = form.shortcut.play;
    next["shortcut"]["next"] = form.shortcut.next;
    next["shortcut"]["increaseSpeed"] = form.shortcut.increaseSpeed;
    next["shortcut"]["decreaseSpeed"] = form.shortcut.decreaseSpeed;
    next["update"]["blockUpdate"] = form.update.blockUpdate;
    return next;
}

void SettingsWindow::setCrashReports(const std::vector<CrashListEntry> &crashes)
{
    m_reports->setCrashes(crashes);
}

int SettingsWindow::currentPage() const
{
    return m_sidebar->current();
}

void SettingsWindow::openPage(int index)
{
    m_sidebar->setCurrent(index);
}

void SettingsWindow::setLanguage(const QString &preference)
{
    m_general->setLanguage(preference);
}

void SettingsWindow::setLegacyCleanup(const QString &title, const QString &description, bool removeEnabled,
                                      bool busy, bool present)
{
    m_general->setLegacyCleanup(title, description, removeEnabled, busy, present);
}

bool SettingsWindow::isLegacyCleanupShown() const
{
    return isVisible() && m_sidebar->current() == m_generalNav;
}

void SettingsWindow::reloadForm()
{
    loadForm(m_services.config.value());
}

void SettingsWindow::showToast(const Toast &toast)
{
    m_toasts->show(toast);
}

void SettingsWindow::notify(ToastType type, const std::string &message)
{
    m_toasts->show(Toast{type, {}, message, kDefaultToastMs});
}

bool SettingsWindow::save()
{
    const Config form = formConfig();
    if (auto valid = ConfigService::validateShortcuts(form.shortcut); !valid) {
        notify(ToastType::Error, valid.error().userMessage);
        return false;
    }
    if (auto saved = m_services.config.replace(mergedJson(form)); !saved) {
        notify(ToastType::Error, trUtf8("Can't save the settings!"));
        return false;
    }
    loadForm(m_services.config.value());
    notify(ToastType::Success, trUtf8("Settings saved and applied."));
    return true;
}

bool SettingsWindow::hasUnsavedChanges() const
{
    return mergedJson(formConfig()) != mergedJson(m_services.config.value());
}

void SettingsWindow::exportSettings()
{
    if (m_busy) {
        return;
    }
    const Config form = formConfig();
    if (auto valid = ConfigService::validateShortcuts(form.shortcut); !valid) {
        notify(ToastType::Error, valid.error().userMessage);
        return;
    }
    const auto path = chooseSettingsExportPath(
        this, QStringLiteral("sky-auto-piano-settings-v%1.json").arg(QLatin1String(kAppVersion)));
    if (!path) {
        return;
    }
    m_busy = true;
    const OrderedJson content = mergedJson(form);
    runInBackground(
        this, [file = *path, content] { return ConfigService::writeSettingsFile(file, content); },
        [this](Result<void> written) {
            m_busy = false;
            if (written) {
                notify(ToastType::Success, trUtf8("Settings exported successfully."));
            } else {
                notify(ToastType::Error, written.error().userMessage);
            }
        });
}

void SettingsWindow::importSettings()
{
    if (m_busy) {
        return;
    }
    const auto path = chooseSettingsImportFile(this);
    if (!path) {
        return;
    }
    m_busy = true;
    runInBackground(
        this, [file = *path] { return ConfigService::readSettingsFile(file); },
        [this](Result<OrderedJson> imported) {
            m_busy = false;
            applyImported(std::move(imported));
        });
}

void SettingsWindow::applyImported(Result<OrderedJson> imported)
{
    if (!imported) {
        notify(ToastType::Error, trUtf8("The selected settings file is invalid."));
        return;
    }
    auto normalized = ConfigService::normalizeImported(m_services.config.raw(), imported.value());
    if (!normalized) {
        notify(ToastType::Error, normalized.error().userMessage);
        return;
    }
    const Config next = ConfigService::fromJson(normalized.value());
    if (auto valid = ConfigService::validateShortcuts(next.shortcut); !valid) {
        notify(ToastType::Error, valid.error().userMessage);
        return;
    }
    if (auto saved = m_services.config.replace(normalized.value()); !saved) {
        notify(ToastType::Error, trUtf8("Can't save the settings!"));
        return;
    }
    loadForm(m_services.config.value());
    notify(ToastType::Success, trUtf8("Settings imported and applied."));
    const auto theme = imported.value().find("appTheme");
    if (theme != imported.value().end() && (*theme == "light" || *theme == "dark")) {
        emit themeImported(*theme == "dark" ? AppTheme::Dark : AppTheme::Light);
    }
}

void SettingsWindow::onBlockUpdatesToggled(bool blocked)
{
    if (!blocked) {
        return;
    }
    QPointer<SettingsWindow> self(this);
    m_blockDialog->ask([self](ConfirmDialog::Choice choice) {
        if (self && choice != ConfirmDialog::Choice::Primary) {
            self->m_updates->setBlockUpdates(false);
        }
    });
}

void SettingsWindow::showUpdateResult(UpdateOverviewCard::State state, const QString &latestVersion)
{
    m_updates->showResult(state, latestVersion);
    m_sidebar->setBadge(m_updatesNav, state == UpdateOverviewCard::State::Available);
}

void SettingsWindow::showWhatsNew()
{
    if (m_loadingWhatsNew) {
        return;
    }
    m_loadingWhatsNew = true;
    const std::string current = m_services.updates.currentVersion();
    QPointer<SettingsWindow> self(this);
    m_services.updates.fetchReleaseNotes(current, [self, current](Result<ReleaseNotes> notes) {
        if (!self) {
            return;
        }
        if (notes) {
            self->m_loadingWhatsNew = false;
            self->m_whatsNew->present(toQString(current), toQString(notes.value().notes), toQString(notes.value().page));
            return;
        }
        self->m_services.updates.checkForUpdates([self, current](Result<VersionCheck> check) {
            if (!self) {
                return;
            }
            self->m_loadingWhatsNew = false;
            if (check) {
                for (const VersionNotes &entry : check.value().history) {
                    if (compareVersions(entry.version, current) <= 0 && !entry.markdown.empty()) {
                        self->m_whatsNew->present(toQString(entry.version), toQString(entry.markdown),
                                                  toQString(UpdateService::releasePageUrl(entry.version)));
                        return;
                    }
                }
            }
            self->notify(ToastType::Warning, trUtf8("Couldn't load the release notes. Check your internet connection and try again."));
        });
    });
}

void SettingsWindow::checkForUpdates()
{
    m_updates->setChecking(true);
    QPointer<SettingsWindow> self(this);
    m_services.updates.checkForUpdates([self](Result<VersionCheck> check) {
        if (!self) {
            return;
        }
        self->m_updates->setChecking(false);
        if (!check) {
            self->showUpdateResult(UpdateOverviewCard::State::Failed);
            self->showToast(Toast{ToastType::Error, {}, check.error().userMessage, kDefaultToastMs});
            return;
        }
        const VersionCheck &info = check.value();
        if (info.available) {
            self->showUpdateResult(UpdateOverviewCard::State::Available, toQString(info.latestVersion));
            emit self->updateAvailable(info);
        } else {
            self->showUpdateResult(UpdateOverviewCard::State::UpToDate);
        }
    });
}

void SettingsWindow::applyPageMargins(int rise)
{
    const int available = width() - m_sidebar->width();
    const int side = std::max(kContentPadding, (available - kContentMaxWidth) / 2);
    m_pageLayout->setContentsMargins(side, kContentPadding + rise, side, kContentBottom);
}

void SettingsWindow::positionOverlays()
{
    m_save->move(width() - kSaveInset - m_save->width(), height() - kSaveInset - m_save->height());
    m_shadows->raise();
    m_sidebar->raise();
    m_save->raise();
    m_toasts->relayout();
}

void SettingsWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setFocus(Qt::OtherFocusReason);
    if (m_sidebar->current() == m_generalNav) {
        emit legacyCleanupShown();
    }
}

void SettingsWindow::closeEvent(QCloseEvent *event)
{
    if (!m_closing && hasUnsavedChanges()) {
        if (formConfig().panel.autoSave) {
            if (!save()) {
                event->ignore();
                return;
            }
        } else {
            event->ignore();
            QPointer<SettingsWindow> self(this);
            m_unsavedDialog->ask([self](ConfirmDialog::Choice choice) {
                if (!self || choice == ConfirmDialog::Choice::Dismissed) {
                    return;
                }
                if (choice == ConfirmDialog::Choice::Primary && !self->save()) {
                    return;
                }
                self->m_closing = true;
                self->close();
            });
            return;
        }
    }
    m_themes->leave();
    QWidget::closeEvent(event);
}

void SettingsWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_pageFade->state() != QAbstractAnimation::Running) {
        applyPageMargins(0);
    }
    positionOverlays();
}

}  // namespace sky
