#pragma once

#include <QObject>

class QTimer;

namespace sky {

enum class SkyWindowState { NotRunning, Minimized, Background, Foreground };

class SkyWindowWatcher final : public QObject {
    Q_OBJECT

public:
    explicit SkyWindowWatcher(QObject *parent = nullptr);
    ~SkyWindowWatcher() override;

    SkyWindowState state() const { return m_state; }
    void refresh();

signals:
    void stateChanged(sky::SkyWindowState state);

private:
    SkyWindowState detect() const;

    QTimer *m_poll;
    void *m_hook = nullptr;
    SkyWindowState m_state = SkyWindowState::NotRunning;
};

}  // namespace sky
