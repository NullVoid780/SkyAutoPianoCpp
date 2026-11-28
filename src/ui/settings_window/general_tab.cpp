#include "ui/settings_window/general_tab.hpp"

#include <QSignalBlocker>
#include <QVBoxLayout>

#include "core/translator.hpp"
#include "core/translator.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/settings_window/settings_row.hpp"
#include "ui/settings_window/tab_header.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/confirm_popup.hpp"
#include "ui/widgets/select_box.hpp"
#include "ui/widgets/toggle_switch.hpp"

namespace sky {

namespace {

constexpr int kHeaderGap = 20;
constexpr int kRowGap = 12;

const char *languageName(const char *code)
{
    const QLatin1String id(code);
    if (id == QLatin1String("en")) {
        return trNoop("English");
    }
    if (id == QLatin1String("vi")) {
        return trNoop("Vietnamese");
    }
    if (id == QLatin1String("zh_CN")) {
        return trNoop("Chinese (Simplified)");
    }
    if (id == QLatin1String("ja")) {
        return trNoop("Japanese");
    }
    return trNoop("Spanish");
}

}  // namespace

GeneralTab::GeneralTab(QWidget *parent)
    : QWidget(parent),
      m_autoSave(new ToggleSwitch(this)),
      m_minimizeOnPlay(new ToggleSwitch(this)),
      m_pauseWhenUnfocused(new ToggleSwitch(this)),
      m_playbackMode(new SelectBox(this)),
      m_language(new SelectBox(this))
{
    auto *header = new TabHeader(QStringLiteral("sliders"), [] { return currentTheme().settings.navIconSliders; },
                                 tr("General Settings"),
                                 tr("Configure basic application behavior and preferences"), this);
    auto *importButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("upload"), tr("Import"));
    auto *exportButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("download"), tr("Export"));
    header->addAction(importButton);
    header->addAction(exportButton);

    m_playbackMode->addOption(tr("Lite"), tr("Lighter on weak PCs, may drift a few ms"));
    m_playbackMode->addOption(tr("Strict"), tr("Most accurate timing, uses more CPU"));
    m_playbackMode->setAccessibleName(tr("Playback Mode"));

    auto *autoSave = new SettingsRow(QStringLiteral("save"), tr("Auto Save Settings"),
                                     tr("Save your changes by itself when you close Settings."), m_autoSave, this);
    auto *minimize = new SettingsRow(QStringLiteral("dash-lg"), tr("Minimize on Play"),
                                     tr("Minimize the window when playback starts."), m_minimizeOnPlay, this);
    auto *pause = new SettingsRow(QStringLiteral("pause-fill"), tr("Pause When Sky Isn't in Front"),
                                  tr("Wait for the game before playing, and pause if you switch to another "
                                                 "window so keys never type into it."),
                                  m_pauseWhenUnfocused, this);
    auto *mode = new SettingsRow(QStringLiteral("music-note-beamed"), tr("Playback Mode"),
                                 tr("Choose how notes are timed while a song plays."), m_playbackMode, this);
    m_language->addOption(tr("Follow Windows"), tr("Use the same language as Windows"));
    for (const LanguageInfo &language : supportedLanguages()) {
        m_language->addOption(QString::fromUtf8(language.nativeName), tr(languageName(language.code)));
    }
    m_language->setAccessibleName(tr("Language"));
    auto *languageRow = new SettingsRow(QStringLiteral("translate"), tr("Language"),
                                        tr("Choose the language of the app. It restarts to switch."), m_language,
                                        this);

    m_legacyRemove = new ActionButton(ActionButton::Kind::Amber, QStringLiteral("trash3"), tr("Remove"));
    m_legacyCleanup = new SettingsRow(QStringLiteral("eraser"), tr("Uninstall the old version"),
                                      tr("Checking the old version..."), m_legacyRemove, this);

    auto *quickStartButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("arrow-clockwise"),
                                              tr("Run again"));
    auto *quickStart = new SettingsRow(QStringLiteral("lightbulb"), tr("Quick Start"),
                                       tr("Go through the first-time setup again."), quickStartButton, this);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(header);
    column->addSpacing(kHeaderGap);
    for (SettingsRow *row :
         {languageRow, autoSave, minimize, pause, mode, m_legacyCleanup, quickStart}) {
        column->addWidget(row);
        if (row != quickStart) {
            column->addSpacing(kRowGap);
        }
    }
    column->addStretch();

    connect(importButton, &QAbstractButton::clicked, this, &GeneralTab::importRequested);
    connect(exportButton, &QAbstractButton::clicked, this, &GeneralTab::exportRequested);
    connect(m_legacyRemove, &QAbstractButton::clicked, this, &GeneralTab::legacyRemovalRequested);
    connect(quickStartButton, &QAbstractButton::clicked, this, &GeneralTab::quickStartRequested);
    connect(m_language, &SelectBox::currentIndexChanged, this, &GeneralTab::onLanguageChanged);
}

void GeneralTab::setLegacyCleanup(const QString &title, const QString &description, bool removeEnabled,
                                  bool busy, bool present)
{
    m_legacyCleanup->setVisible(present);
    m_legacyCleanup->setTitle(title);
    m_legacyCleanup->setDescription(description);
    m_legacyRemove->setEnabled(removeEnabled && !busy);
    m_legacyRemove->setBusy(busy, tr("Uninstalling..."));
}

void GeneralTab::setLanguage(const QString &preference)
{
    int index = 0;
    const auto &languages = supportedLanguages();
    for (std::size_t i = 0; i < languages.size(); ++i) {
        if (preference == QLatin1String(languages[i].code)) {
            index = static_cast<int>(i) + 1;
        }
    }
    const QSignalBlocker blocker(m_language);
    m_language->setCurrentIndex(index);
}

void GeneralTab::onLanguageChanged(int index)
{
    const auto &languages = supportedLanguages();
    const QString preference = index <= 0 || index > static_cast<int>(languages.size())
                                   ? QString()
                                   : QString::fromLatin1(languages[static_cast<std::size_t>(index - 1)].code);
    emit languageChosen(preference);
    if (resolveLanguage(preference) == currentLanguage()) {
        return;
    }
    if (!m_confirm) {
        m_confirm = new ConfirmPopup(window());
    }
    QPointer<GeneralTab> self(this);
    m_confirm->ask(tr("Restart Sky Auto Piano now to switch the language?"), [self] {
        if (self) {
            emit self->restartRequested();
        }
    });
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
