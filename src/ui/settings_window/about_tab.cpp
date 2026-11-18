#include "ui/settings_window/about_tab.hpp"

#include <QVBoxLayout>

#include "ui/settings_window/action_button.hpp"
#include "ui/settings_window/app_info_card.hpp"
#include "ui/settings_window/settings_row.hpp"
#include "ui/settings_window/tab_header.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeaderGap = 20;
constexpr int kCardGap = 16;
constexpr int kRowGap = 12;

ActionButton *linkButton(const QString &iconId, const QString &label)
{
    return new ActionButton(ActionButton::Kind::Subtle, iconId, label);
}

}  // namespace

AboutTab::AboutTab(const QString &version, QWidget *parent) : QWidget(parent)
{
    auto *header = new TabHeader(QStringLiteral("info-circle"), [] { return currentTheme().hoverColor; },
                                 QStringLiteral("About"), QStringLiteral("Information about Sky Auto Piano"), this);
    auto *card = new AppInfoCard(version, this);

    auto *originalButton = linkButton(QStringLiteral("box-arrow-up-right"), QStringLiteral("Open"));
    auto *sourceButton = linkButton(QStringLiteral("box-arrow-up-right"), QStringLiteral("Open"));
    auto *dataButton = linkButton(QStringLiteral("folder2"), QStringLiteral("Open folder"));
    auto *licensesButton = linkButton(QStringLiteral("folder2"), QStringLiteral("Open folder"));

    auto *original = new SettingsRow(QStringLiteral("lightbulb"), QStringLiteral("Original App"),
                                     QStringLiteral("Based on Sky Auto Piano by HerokeyVN."), originalButton, this);
    auto *source = new SettingsRow(QStringLiteral("edit-note"), QStringLiteral("Source Code"),
                                   QStringLiteral("SkyAutoPianoCpp, rewritten in C++ and Qt."), sourceButton, this);
    auto *dataRow = new SettingsRow(QStringLiteral("folder2"), QStringLiteral("Data Folder"),
                                 QStringLiteral("Where your songs and settings are saved."), dataButton, this);
    auto *licenses = new SettingsRow(QStringLiteral("shield-check"), QStringLiteral("Licenses"),
                                     QStringLiteral("Qt, Roboto, Bootstrap Icons and other open-source parts."),
                                     licensesButton, this);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(header);
    column->addSpacing(kHeaderGap);
    column->addWidget(card);
    column->addSpacing(kCardGap);
    for (SettingsRow *row : {original, source, dataRow, licenses}) {
        column->addWidget(row);
        if (row != licenses) {
            column->addSpacing(kRowGap);
        }
    }
    column->addStretch();

    connect(originalButton, &QAbstractButton::clicked, this, &AboutTab::originalProjectRequested);
    connect(sourceButton, &QAbstractButton::clicked, this, &AboutTab::sourceCodeRequested);
    connect(dataButton, &QAbstractButton::clicked, this, &AboutTab::dataFolderRequested);
    connect(licensesButton, &QAbstractButton::clicked, this, &AboutTab::licensesRequested);
}

}  // namespace sky
