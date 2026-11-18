#pragma once

#include <QWidget>

class QLabel;

namespace sky {

class UpdateOverviewCard final : public QWidget {
    Q_OBJECT

public:
    enum class State { Unknown, Checking, UpToDate, Available, Failed };

    explicit UpdateOverviewCard(const QString &currentVersion, QWidget *parent = nullptr);

    void setState(State state, const QString &latestVersion = QString());

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString pillText() const;
    void refresh();

    QWidget *m_tileSpace;
    QWidget *m_pillSpace;
    QLabel *m_version;
    QString m_currentVersion;
    QString m_latestVersion;
    State m_state = State::Unknown;
};

}  // namespace sky
