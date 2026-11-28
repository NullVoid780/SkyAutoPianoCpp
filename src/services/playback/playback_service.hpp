#pragma once

#include <memory>

#include "model/config.hpp"
#include "services/playback/playback_port.hpp"
#include "services/playback/player.hpp"
#include "skyinput/keyboard.hpp"
#include "skyinput/sender.hpp"

namespace sky {

class ConfigService;
class LitePlayer;
class PlayerBase;
class StrictPlayer;

class PlaybackService final : public IPlaybackPort {
    Q_OBJECT

public:
    explicit PlaybackService(ConfigService &config,
                             std::unique_ptr<skyinput::ISender> sender = nullptr,
                             QObject *parent = nullptr);
    ~PlaybackService() override;

    void play(const PlayRequest &request) override;
    void stop() override;
    void shutdown();
    bool isPlaying() const override;
    void setHeld(bool held);
    bool isHeld() const { return m_held; }
    PlaybackMode activeMode() const { return m_activeMode; }

private:
    PlayerBase &playerFor(PlaybackMode mode);
    void connectPlayer(PlayerBase &player);

    ConfigService &m_config;
    std::unique_ptr<skyinput::ISender> m_sender;
    std::unique_ptr<skyinput::Keyboard> m_strictKeyboard;
    std::unique_ptr<skyinput::Keyboard> m_liteKeyboard;
    std::unique_ptr<StrictPlayer> m_strict;
    std::unique_ptr<LitePlayer> m_lite;
    PlaybackMode m_activeMode;
    bool m_held = false;
};

}  // namespace sky
