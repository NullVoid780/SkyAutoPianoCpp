#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "services/update/update_service.hpp"

class QWidget;

namespace sky {

class ToastService;
class TrayNotification;
class UpdatePrompt;

class UpdateController final : public QObject {
    Q_OBJECT

public:
    UpdateController(UpdateService &updates, ToastService &toasts, TrayNotification &tray, QObject *parent = nullptr);

    void start(QWidget *window, std::function<bool()> enabled);
    void offer(QWidget *window, const VersionCheck &check);

signals:
    void restartRequested();

private:
    void checkAndOffer();
    void onTick();
    void onOnline();
    void scheduleRetry();
    void watchNetwork();
    bool promptOpen() const;
    UpdatePrompt *promptFor(QWidget *window);
    void onProgress(int percent);
    void onReady();
    void onFailed(const QString &message);

    UpdateService &m_updates;
    ToastService &m_toasts;
    TrayNotification &m_tray;
    QPointer<QWidget> m_window;
    std::function<bool()> m_enabled;
    QTimer m_timer;
    QTimer m_retry;
    QDateTime m_lastSuccess;
    int m_failures = 0;
    bool m_checking = false;
    std::string m_snoozedVersion;
    VersionCheck m_check;
    std::vector<QPointer<UpdatePrompt>> m_prompts;
};

}  // namespace sky
