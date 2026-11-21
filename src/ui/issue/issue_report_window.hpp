#pragma once

#include <functional>

#include <QWidget>

class QLabel;
class QPlainTextEdit;

namespace sky {

class ActionButton;
class SettingsRow;
class ToggleSwitch;

class IssueReportWindow final : public QWidget {
    Q_OBJECT

public:
    struct Choices {
        QString description;
        bool library = true;
        bool failedFiles = true;
    };

    explicit IssueReportWindow(QWidget *owner);

    void setSummary(qint64 durationMs, int events, int screenshots, int failedFiles);

signals:
    void saveRequested(const sky::IssueReportWindow::Choices &choices);
    void discardRequested();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QLabel *m_summary;
    QPlainTextEdit *m_description;
    ToggleSwitch *m_library;
    ToggleSwitch *m_failedFiles;
    SettingsRow *m_failedRow;
    bool m_decided = false;
};

}  // namespace sky
