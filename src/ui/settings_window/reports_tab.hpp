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

    void setCrashes(const std::vector<CrashListEntry> &crashes);

signals:
    void recordIssueRequested();
    void saveCrashReportRequested(const QString &id);
    void crashFolderRequested();

private:
    QWidget *m_crashHost;
    QVBoxLayout *m_crashList;
    QLabel *m_crashCount;
};

}  // namespace sky
