#include "ui/settings_window/updates_tab.hpp"

#include <QVBoxLayout>

#include "core/translator.hpp"
#include "ui/widgets/action_button.hpp"
#include "ui/widgets/settings_row.hpp"
#include "ui/settings_window/tab_header.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/toggle_switch.hpp"

namespace sky {

namespace {

constexpr int kHeaderGap = 20;
constexpr int kOverviewGap = 16;
constexpr int kRowGap = 12;

}  // namespace

UpdatesTab::UpdatesTab(const QString &currentVersion, QWidget *parent)
    : QWidget(parent),
      m_overview(new UpdateOverviewCard(currentVersion, this)),
      m_automatic(new ToggleSwitch(this)),
      m_check(new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("play-fill"), tr("Check Now"), this))
{
    auto *header = new TabHeader(QStringLiteral("update"), [] { return currentTheme().settings.navIconRefresh; },
                                 tr("Updates"),
                                 tr("Manage application updates and version information"), this);

    auto *automatic = new SettingsRow(QStringLiteral("lightning"), tr("Automatic Updates"),
                                      tr("Automatically check for updates when the app starts."),
                                      m_automatic, this);
    auto *check = new SettingsRow(QStringLiteral("update"), tr("Check for Updates"),
                                  tr("Manually check for new updates."), m_check, this);
    auto *whatsNewButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("list"), tr("Show"));
    auto *whatsNew = new SettingsRow(QStringLiteral("info-circle"), tr("What's New"),
                                     tr("See what changed in this version."), whatsNewButton, this);
    auto *join = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("box-arrow-up-right"),
                                  tr("Join Discord"));
    auto *community = new SettingsRow(QStringLiteral("discord"), tr("Community & Bug Reports"),
                                      tr("Join our Discord server to report bugs and connect with other users."),
                                      join, this);
    community->setIconTile([] { return currentTheme().settings.discordFill; },
                           [] { return currentTheme().accent.white; });

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(header);
    column->addSpacing(kHeaderGap);
    column->addWidget(m_overview);
    column->addSpacing(kOverviewGap);
    column->addWidget(automatic);
    column->addSpacing(kRowGap);
    column->addWidget(check);
    column->addSpacing(kRowGap);
    column->addWidget(whatsNew);
    column->addSpacing(kRowGap);
    column->addWidget(community);
    column->addStretch();

    connect(m_automatic, &QAbstractButton::toggled, this, [this](bool on) { emit blockUpdatesToggled(!on); });
    connect(m_check, &QAbstractButton::clicked, this, [this] {
        if (!m_checking) {
            emit checkRequested();
        }
    });
    connect(whatsNewButton, &QAbstractButton::clicked, this, &UpdatesTab::whatsNewRequested);
    connect(join, &QAbstractButton::clicked, this, &UpdatesTab::communityRequested);
}

void UpdatesTab::load(const Config &config)
{
    m_automatic->setCheckedImmediately(!config.update.blockUpdate);
}

void UpdatesTab::collect(Config &config) const
{
    config.update.blockUpdate = !m_automatic->isChecked();
}

void UpdatesTab::setBlockUpdates(bool blocked)
{
    m_automatic->setChecked(!blocked);
}

void UpdatesTab::setChecking(bool checking)
{
    m_checking = checking;
    m_check->setBusy(checking, tr("Checking..."));
    if (checking) {
        m_overview->setState(UpdateOverviewCard::State::Checking);
    }
}

void UpdatesTab::showResult(UpdateOverviewCard::State state, const QString &latestVersion)
{
    m_overview->setState(state, latestVersion);
}

}  // namespace sky
