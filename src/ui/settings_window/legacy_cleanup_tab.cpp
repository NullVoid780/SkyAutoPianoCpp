#include "ui/settings_window/legacy_cleanup_tab.hpp"

#include <QVBoxLayout>

#include "core/translator.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/settings_window/settings_row.hpp"
#include "ui/settings_window/tab_header.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeaderGap = 20;

}  // namespace

LegacyCleanupTab::LegacyCleanupTab(QWidget *parent) : QWidget(parent)
{
    auto *header = new TabHeader(QStringLiteral("eraser"), [] { return currentTheme().hoverColor; },
                                 tr("Remove Old Electron Version"),
                                 tr("Uninstall the previous version from this PC once your songs are safe "
                                    "in this library."),
                                 this);
    m_remove = new ActionButton(ActionButton::Kind::Amber, QStringLiteral("trash3"), tr("Remove"));
    m_cleanup = new SettingsRow(QStringLiteral("eraser"), tr("Remove Old Electron Version"),
                                tr("Checking the old version..."), m_remove, this);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(header);
    column->addSpacing(kHeaderGap);
    column->addWidget(m_cleanup);
    column->addStretch();

    connect(m_remove, &QAbstractButton::clicked, this, &LegacyCleanupTab::legacyRemovalRequested);
}

void LegacyCleanupTab::setLegacyCleanup(const QString &description, bool removeEnabled, bool busy)
{
    m_cleanup->setDescription(description);
    m_remove->setEnabled(removeEnabled && !busy);
    m_remove->setBusy(busy, tr("Uninstalling..."));
}

}  // namespace sky
