#pragma once

#include <QObject>

class QLocalServer;

namespace sky {

class SingleInstance final : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;

    bool acquire();
    void listen();
    void release();
    static void notifyRunningInstance();

signals:
    void activationRequested();

private:
    void *m_mutex = nullptr;
    QLocalServer *m_server = nullptr;
};

}  // namespace sky
