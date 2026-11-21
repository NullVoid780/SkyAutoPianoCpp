#include "ui/settings_window/reports_tab.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "crash/pending_reports.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/settings_window/settings_row.hpp"
#include "ui/settings_window/tab_header.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeaderGap = 20;
constexpr int kSectionGap = 28;
constexpr int kTitleGap = 10;
constexpr int kRowGap = 12;

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *title = new QLabel(text, parent);
    title->setObjectName(QStringLiteral("ThemeGroupTitle"));
    return title;
}

QLabel *sectionNote(const QString &text, QWidget *parent)
{
    auto *note = new QLabel(text, parent);
    note->setObjectName(QStringLiteral("SettingsRowDescription"));
    note->setWordWrap(true);
    return note;
}

}  // namespace

ReportsTab::ReportsTab(QWidget *parent)
    : QWidget(parent),
      m_crashHost(new QWidget(this)),
      m_crashList(new QVBoxLayout(m_crashHost)),
      m_crashCount(nullptr),
      m_clearButton(nullptr)
{
    auto *header = new TabHeader(QStringLiteral("bug"), [] { return currentTheme().hoverColor; },
                                 QStringLiteral("Reports"),
                                 QStringLiteral("Found a problem? Send the developer everything needed to fix it."), this);

    auto *recordButton =
        new ActionButton(ActionButton::Kind::Primary, QStringLiteral("play-fill"), QStringLiteral("Start recording"));
    auto *record = new SettingsRow(QStringLiteral("bug"), QStringLiteral("Record an Issue"),
                                   QStringLiteral("A button does nothing, something looks wrong or a song won't import? "
                                                  "Start recording, show the problem, then save the report."),
                                   recordButton, this);

    auto *crashTitleRow = new QHBoxLayout();
    crashTitleRow->setContentsMargins(0, 0, 0, 0);
    crashTitleRow->setSpacing(8);
    crashTitleRow->addWidget(sectionTitle(QStringLiteral("Crash Reports"), this));
    m_crashCount = sectionNote(QString(), this);
    crashTitleRow->addWidget(m_crashCount);
    crashTitleRow->addStretch();
    auto *clearButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("trash3"), QStringLiteral("Delete all"));
    m_clearButton = clearButton;
    crashTitleRow->addWidget(clearButton);
    auto *folderButton = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("folder2"), QStringLiteral("Open folder"));
    crashTitleRow->addWidget(folderButton);

    m_crashList->setContentsMargins(0, 0, 0, 0);
    m_crashList->setSpacing(kRowGap);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(header);
    column->addSpacing(kHeaderGap);
    column->addWidget(record);
    column->addSpacing(8);
    column->addWidget(sectionNote(QStringLiteral("The report includes screenshots of Sky Auto Piano's own windows, your "
                                                 "clicks and keys, the app log, your settings, themes and window layout. "
                                                 "Nothing is sent by itself: you save a file and share it yourself."),
                                  this));
    column->addSpacing(kSectionGap);
    column->addLayout(crashTitleRow);
    column->addSpacing(kTitleGap);
    column->addWidget(m_crashHost);
    column->addSpacing(kTitleGap);
    column->addWidget(sectionNote(QStringLiteral("Crash reports older than %1 days are deleted by themselves.")
                                      .arg(kCrashReportMaxAgeDays),
                                  this));
    column->addStretch();

    connect(recordButton, &QAbstractButton::clicked, this, &ReportsTab::recordIssueRequested);
    connect(folderButton, &QAbstractButton::clicked, this, &ReportsTab::crashFolderRequested);
    connect(clearButton, &QAbstractButton::clicked, this, &ReportsTab::clearCrashReportsRequested);
    setCrashes({});
}

void ReportsTab::setCrashes(const std::vector<CrashListEntry> &crashes)
{
    while (QLayoutItem *item = m_crashList->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    m_clearButton->setVisible(!crashes.empty());
    m_crashCount->setText(crashes.empty() ? QString()
                          : crashes.size() == 1 ? QStringLiteral("1 saved")
                                                : QStringLiteral("%1 saved").arg(crashes.size()));
    if (crashes.empty()) {
        m_crashList->addWidget(sectionNote(QStringLiteral("No crash reports. If Sky Auto Piano ever crashes, the report "
                                                          "appears here so you can save it for the developer."),
                                           m_crashHost));
        return;
    }
    for (const CrashListEntry &crash : crashes) {
        auto *button =
            new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("download"), QStringLiteral("Save report"));
        auto *row = new SettingsRow(QStringLiteral("exclamation-triangle"), crash.title, crash.details, button, m_crashHost);
        const QString id = crash.id;
        connect(button, &QAbstractButton::clicked, this, [this, id] { emit saveCrashReportRequested(id); });
        m_crashList->addWidget(row);
    }
}

}  // namespace sky
