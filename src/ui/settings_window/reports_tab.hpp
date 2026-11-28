#pragma once

#include <vector>

#include <QString>
#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace sky {

struct CrashListEntry {
    QString id;
    QString title;
    QString details;
};

class ReportsTab final : public QWidget {
    Q_OBJECT

public:
    explicit ReportsTab(QWidget *parent = nullptr);

    void setCrashes(const std::vector<CrashListEntry> &crashes, int retentionDays = 30);

signals:
    void recordIssueRequested();
    void saveCrashReportRequested(const QString &id);
    void crashFolderRequested();
    void clearCrashReportsRequested();

private:
    QWidget *m_crashHost;
    QVBoxLayout *m_crashList;
    QLabel *m_crashCount;
    QLabel *m_retentionNote;
    QWidget *m_clearButton;
};

}  // namespace sky
