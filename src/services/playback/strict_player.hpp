#pragma once

#include "services/playback/player_base.hpp"

namespace sky {

class StrictPlayer final : public PlayerBase {
    Q_OBJECT

public:
    StrictPlayer(ConfigService &config, skyinput::Keyboard &keyboard, QObject *parent = nullptr);
    ~StrictPlayer() override;

protected:
    void playback(const PlayRequest &request, quint64 session) override;
    void trackProgress(const PlayRequest &request, quint64 session) override;
    void releaseHeldKeys() override;
};

}  // namespace sky
