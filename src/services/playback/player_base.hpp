#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <QObject>

#include "services/playback/player.hpp"
#include "skyinput/key_code.hpp"
#include "skyinput/keyboard.hpp"

namespace sky {

class ConfigService;

class PlayerBase : public QObject, public IPlayer {
    Q_OBJECT

public:
    PlayerBase(ConfigService &config, skyinput::Keyboard &keyboard, QObject *parent = nullptr);
    ~PlayerBase() override;

    void start(const PlayRequest &request) final;
    void stop(bool emitStopEvent, bool manual) final;
    bool isPlaying() const final;

signals:
    void progress(int second);
    void stopped(bool manual);

protected:
    virtual void playback(const PlayRequest &request, quint64 session) = 0;
    virtual void trackProgress(const PlayRequest &request, quint64 session) = 0;
    virtual void releaseHeldKeys() = 0;

    void shutdown();
    bool isActive(quint64 session) const;
    void finishNaturally(quint64 session);
    void emitProgress(quint64 session, int second);
    std::optional<skyinput::KeyCode> keyCodeFor(const std::string &name) const;

    ConfigService &config() { return m_config; }
    skyinput::Keyboard &keyboard() { return m_keyboard; }

private:
    void joinWorkers();
    void runGuarded(const char *name, int priority, const std::function<void()> &body);

    ConfigService &m_config;
    skyinput::Keyboard &m_keyboard;
    std::mutex m_stateMutex;
    std::atomic<quint64> m_session{0};
    std::atomic<bool> m_playing{false};
    std::thread m_playbackThread;
    std::thread m_progressThread;
};

}  // namespace sky
