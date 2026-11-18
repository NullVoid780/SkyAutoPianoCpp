#include "services/playback/playback_service.hpp"

#include "core/logger.hpp"
#include "services/settings/config_service.hpp"
#include "services/playback/lite_player.hpp"
#include "services/playback/strict_player.hpp"
#include "skyinput/hardware_sender.hpp"
#include "skyinput/window_info.hpp"

namespace sky {

PlaybackService::PlaybackService(ConfigService &config, std::unique_ptr<skyinput::ISender> sender, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_sender(sender ? std::move(sender) : std::make_unique<skyinput::HardwareSender>()),
      m_strictKeyboard(std::make_unique<skyinput::Keyboard>(*m_sender)),
      m_liteKeyboard(std::make_unique<skyinput::Keyboard>(*m_sender)),
      m_strict(std::make_unique<StrictPlayer>(config, *m_strictKeyboard)),
      m_lite(std::make_unique<LitePlayer>(config, *m_liteKeyboard)),
      m_activeMode(config.panel().playbackMode)
{
    connectPlayer(*m_strict);
    connectPlayer(*m_lite);
}

PlaybackService::~PlaybackService()
{
    shutdown();
}

void PlaybackService::connectPlayer(PlayerBase &player)
{
    connect(&player, &PlayerBase::progress, this, &PlaybackService::progress);
    connect(&player, &PlayerBase::stopped, this, &PlaybackService::stopped);
}

PlayerBase &PlaybackService::playerFor(PlaybackMode mode)
{
    if (mode == PlaybackMode::Lite) {
        return *m_lite;
    }
    return *m_strict;
}

void PlaybackService::play(const PlayRequest &request)
{
    const PlaybackMode mode = m_config.panel().playbackMode;
    PlayerBase &target = playerFor(mode);
    PlayerBase &other = playerFor(mode == PlaybackMode::Lite ? PlaybackMode::Strict : PlaybackMode::Lite);
    m_sender->setTarget(skyinput::findWindow(L"Sky"));
    m_activeMode = mode;
    other.stop(false, true);
    Logger::info("playback", std::string("start mode=") + (mode == PlaybackMode::Lite ? "lite" : "strict") +
                                 " notes=" + std::to_string(request.notes.size()) +
                                 " steps=" + std::to_string(request.keyMap.size()));
    emit started();
    target.start(request);
}

void PlaybackService::stop()
{
    PlayerBase &active = playerFor(m_activeMode);
    PlayerBase &inactive = playerFor(m_activeMode == PlaybackMode::Lite ? PlaybackMode::Strict : PlaybackMode::Lite);
    active.stop(true, true);
    inactive.stop(false, true);
}

void PlaybackService::shutdown()
{
    if (m_strict) {
        m_strict->stop(false, true);
    }
    if (m_lite) {
        m_lite->stop(false, true);
    }
}

bool PlaybackService::isPlaying() const
{
    return m_strict->isPlaying() || m_lite->isPlaying();
}

}  // namespace sky
