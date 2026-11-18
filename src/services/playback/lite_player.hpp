#pragma once

#include <optional>
#include <string>

#include "services/playback/player_base.hpp"
#include "services/playback/release_scheduler.hpp"

namespace sky {

inline constexpr double kLiteMinPressMs = 25.0;
inline constexpr double kLiteReleaseTrimMs = 35.0;

class LitePlayer final : public PlayerBase {
    Q_OBJECT

public:
    LitePlayer(ConfigService &config, skyinput::Keyboard &keyboard, QObject *parent = nullptr);
    ~LitePlayer() override;

protected:
    void playback(const PlayRequest &request, quint64 session) override;
    void trackProgress(const PlayRequest &request, quint64 session) override;
    void releaseHeldKeys() override;

private:
    void playNotes(const std::vector<PlaybackNote> &notes, int startSec, quint64 session);
    void playKeyMap(const KeyMap &keyMap, quint64 session);
    void sendKey(const std::string &rawKey, std::optional<double> durationMs);
    bool sleepFor(double milliseconds, quint64 session);

    ReleaseScheduler m_scheduler;
};

}  // namespace sky
