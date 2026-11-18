#pragma once

#include <QObject>

class QSystemTrayIcon;

namespace sky {

class TrayNotification final : public QObject {
    Q_OBJECT

public:
    explicit TrayNotification(QObject *parent = nullptr);

    void show(const QString &title, const QString &body);

signals:
    void clicked();

private:
    QSystemTrayIcon *m_icon;
};

}  // namespace sky
