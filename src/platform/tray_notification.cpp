#include "platform/tray_notification.hpp"

#include <QIcon>
#include <QSystemTrayIcon>

namespace sky {

TrayNotification::TrayNotification(QObject *parent)
    : QObject(parent), m_icon(new QSystemTrayIcon(QIcon(QStringLiteral(":/icon/Icon9.ico")), this))
{
    m_icon->setToolTip(QStringLiteral("Sky Auto Piano"));
    connect(m_icon, &QSystemTrayIcon::messageClicked, this, [this] {
        m_icon->hide();
        emit clicked();
    });
    connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            m_icon->hide();
            emit clicked();
        }
    });
}

void TrayNotification::show(const QString &title, const QString &body)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    m_icon->show();
    m_icon->showMessage(title, body, m_icon->icon());
}

}  // namespace sky
