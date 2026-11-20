#include "ui/settings_window/general_tab.hpp"

#include <QSignalBlocker>
#include <QVBoxLayout>

#include "ui/settings_window/action_button.hpp"
#include "ui/settings_window/settings_row.hpp"
#include "ui/settings_window/tab_header.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/select_box.hpp"
#include "ui/widgets/toggle_switch.hpp"

namespace sky {

namespace {

constexpr int kHeaderGap = 20;
constexpr int kRowGap = 12;

}  // namespace

GeneralTab::GeneralTab(QWidget *parent)
    : QWidget(parent),
      m_autoSave(new ToggleSwitch(this)),
      m_minimizeOnPlay(new ToggleSwitch(this)),
      m_pauseWhenUnfocused(new ToggleSwitch(this)),
      m_playbackMode(new SelectBox(this))
{
    auto *header = new TabHeader(QStringLiteral("sliders"), [] { return currentTheme().settings.navIconSliders; },
                                 QStringLiteral("General Settings"),
                                 QStringLiteral("Configure basic application behavior and preferences"), this);
    auto *importButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("upload"), QStringLiteral("Import"));
    auto *exportButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("download"), QStringLiteral("Export"));
    header->addAction(importButton);
    header->addAction(exportButton);

    m_playbackMode->addOption(QStringLiteral("Lite"), QStringLiteral("Lighter on weak PCs, may drift a few ms"));
    m_playbackMode->addOption(QStringLiteral("Strict"), QStringLiteral("Most accurate timing, uses more CPU"));
    m_playbackMode->setAccessibleName(QStringLiteral("Playback Mode"));

    auto *autoSave = new SettingsRow(QStringLiteral("save"), QStringLiteral("Auto Save Settings"),
                                     QStringLiteral("Automatically save settings to the control panel."), m_autoSave, this);
    auto *minimize = new SettingsRow(QStringLiteral("dash-lg"), QStringLiteral("Minimize on Play"),
                                     QStringLiteral("Minimize the window when playback starts."), m_minimizeOnPlay, this);
    auto *pause = new SettingsRow(QStringLiteral("pause-fill"), QStringLiteral("Pause When Sky Isn't in Front"),
                                  QStringLiteral("Wait for the game before playing, and pause if you switch to another "
                                                 "window so keys never type into it."),
                                  m_pauseWhenUnfocused, this);
    auto *mode = new SettingsRow(QStringLiteral("music-note-beamed"), QStringLiteral("Playback Mode"),
                                 QStringLiteral("Choose how notes are timed while a song plays."), m_playbackMode, this);
    auto *importPrevious = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("download"), QStringLiteral("Import"));
    auto *previous = new SettingsRow(QStringLiteral("music-note-list"), QStringLiteral("Import Previous Songs"),
                                     QStringLiteral("Copy all songs from the Electron version of Sky Auto Piano into this library."),
                                     importPrevious, this);

    auto *quickStartButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("arrow-clockwise"),
                                              QStringLiteral("Run again"));
    auto *quickStart = new SettingsRow(QStringLiteral("lightbulb"), QStringLiteral("Quick Start"),
                                       QStringLiteral("Go through the first-time setup again."), quickStartButton, this);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(header);
    column->addSpacing(kHeaderGap);
    for (SettingsRow *row : {autoSave, minimize, pause, mode, previous, quickStart}) {
        column->addWidget(row);
        if (row != quickStart) {
            column->addSpacing(kRowGap);
        }
    }
    column->addStretch();

    connect(importButton, &QAbstractButton::clicked, this, &GeneralTab::importRequested);
    connect(exportButton, &QAbstractButton::clicked, this, &GeneralTab::exportRequested);
    connect(importPrevious, &QAbstractButton::clicked, this, &GeneralTab::importPreviousRequested);
    connect(quickStartButton, &QAbstractButton::clicked, this, &GeneralTab::quickStartRequested);
}

void GeneralTab::load(const Config &config)
{
    m_autoSave->setCheckedImmediately(config.panel.autoSave);
    m_minimizeOnPlay->setCheckedImmediately(config.panel.minimizeOnPlay);
    m_pauseWhenUnfocused->setCheckedImmediately(config.panel.pauseWhenUnfocused);
    const QSignalBlocker blocker(m_playbackMode);
    m_playbackMode->setCurrentIndex(config.panel.playbackMode == PlaybackMode::Strict ? 1 : 0);
}

void GeneralTab::collect(Config &config) const
{
    config.panel.autoSave = m_autoSave->isChecked();
    config.panel.minimizeOnPlay = m_minimizeOnPlay->isChecked();
    config.panel.pauseWhenUnfocused = m_pauseWhenUnfocused->isChecked();
    config.panel.playbackMode = m_playbackMode->currentIndex() == 1 ? PlaybackMode::Strict : PlaybackMode::Lite;
}

}  // namespace sky
