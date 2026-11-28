#include "ui/issue/issue_report_window.hpp"

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "core/translator.hpp"
#include "platform/window_attributes.hpp"
#include "ui/settings_window/action_button.hpp"
#include "ui/settings_window/settings_row.hpp"
#include "ui/theme_manager.hpp"
#include "ui/widgets/toggle_switch.hpp"

namespace sky {

namespace {

constexpr int kPadding = 16;
constexpr int kRowGap = 8;
constexpr int kSectionGap = 12;

QLabel *label(const QString &text, const char *objectName, QWidget *parent)
{
    auto *result = new QLabel(text, parent);
    result->setObjectName(QString::fromLatin1(objectName));
    result->setWordWrap(true);
    return result;
}

ToggleSwitch *checkedSwitch(QWidget *parent)
{
    auto *toggle = new ToggleSwitch(parent);
    toggle->setCheckedImmediately(true);
    return toggle;
}

}  // namespace

IssueReportWindow::IssueReportWindow(QWidget *owner)
    : QWidget(owner, Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint),
      m_summary(label(QString(), "SettingsRowDescription", this)),
      m_description(new QPlainTextEdit(this)),
      m_library(checkedSwitch(this)),
      m_failedFiles(checkedSwitch(this)),
      m_failedRow(nullptr)
{
    setObjectName(QStringLiteral("IssueReportWindow"));
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Report an Issue"));
    setWindowIcon(QIcon(QStringLiteral(":/icon/Icon9.ico")));
    resize(560, 560);
    setMinimumSize(480, 460);

    m_description->setObjectName(QStringLiteral("IssueDescription"));
    m_description->setPlaceholderText(
        tr("What went wrong? For example: \"The Import button does nothing after I pick a MIDI file.\""));
    m_description->setAccessibleName(tr("Describe the problem"));

    auto *library = new SettingsRow(QStringLiteral("music-note-list"), tr("Song list"),
                                    tr("Names and authors of your songs, so the developer sees the same library."),
                                    m_library, this);
    m_failedRow = new SettingsRow(QStringLiteral("upload"), tr("Files that failed to import"),
                                  tr("A copy of the song files that showed an error."), m_failedFiles, this);
    auto *note = label(tr("Always included: screenshots of Sky Auto Piano's windows, your clicks and "
                                      "keys, the app log, your settings, themes, playlists, window layout and PC details "
                                      "(Windows version, screens, memory). Nothing is sent by itself: you save a file "
                                      "and share it yourself."),
                       "SettingsRowDescription", this);

    auto *discard = new ActionButton(ActionButton::Kind::Subtle, QStringLiteral("trash3"), tr("Discard"));
    auto *save = new ActionButton(ActionButton::Kind::Primary, QStringLiteral("floppy"), tr("Save report"));
    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(save, QDialogButtonBox::AcceptRole);
    buttons->addButton(discard, QDialogButtonBox::RejectRole);

    auto *column = new QVBoxLayout(this);
    column->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    column->setSpacing(0);
    column->addWidget(label(tr("Report an Issue"), "ReportWindowTitle", this));
    column->addSpacing(4);
    column->addWidget(m_summary);
    column->addSpacing(kSectionGap);
    column->addWidget(label(tr("Describe the problem"), "ColorPickerCaption", this));
    column->addSpacing(4);
    column->addWidget(m_description, 1);
    column->addSpacing(kSectionGap);
    column->addWidget(label(tr("Include"), "ColorPickerCaption", this));
    column->addSpacing(4);
    for (SettingsRow *row : {library, m_failedRow}) {
        column->addWidget(row);
        column->addSpacing(kRowGap);
    }
    column->addWidget(note);
    column->addSpacing(kSectionGap);
    column->addWidget(buttons);

    connect(save, &QAbstractButton::clicked, this, [this] {
        Choices choices;
        choices.description = m_description->toPlainText();
        choices.library = m_library->isChecked();
        choices.failedFiles = m_failedRow->isVisible() && m_failedFiles->isChecked();
        m_decided = true;
        emit saveRequested(choices);
    });
    connect(discard, &QAbstractButton::clicked, this, [this] {
        m_decided = true;
        emit discardRequested();
        close();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { applyThemeTitleBar(this); });
    applyThemeTitleBar(this);
}

void IssueReportWindow::setSummary(qint64 durationMs, int events, int screenshots, int failedFiles)
{
    const qint64 seconds = durationMs / 1000;
    m_summary->setText(tr("Recorded %1:%2 with %3 steps and %4 screenshots.")
                           .arg(seconds / 60)
                           .arg(seconds % 60, 2, 10, QLatin1Char('0'))
                           .arg(events)
                           .arg(screenshots));
    m_failedRow->setVisible(failedFiles > 0);
}

void IssueReportWindow::closeEvent(QCloseEvent *event)
{
    if (!m_decided) {
        m_decided = true;
        emit discardRequested();
    }
    QWidget::closeEvent(event);
}

}  // namespace sky
