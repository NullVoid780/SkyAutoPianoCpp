#include "services/playback/strict_player.hpp"

#include <cmath>
#include <unordered_map>

#include "core/precise_sleep.hpp"
#include "model/key_table.hpp"
#include "services/settings/config_service.hpp"
#include "services/playback/song_clock.hpp"
#include "services/playback/timeline_builder.hpp"

namespace sky {

StrictPlayer::StrictPlayer(ConfigService &config, skyinput::Keyboard &keyboard, QObject *parent)
    : PlayerBase(config, keyboard, parent)
{
}

StrictPlayer::~StrictPlayer()
{
    shutdown();
    releaseHeldKeys();
}

void StrictPlayer::releaseHeldKeys()
{
    keyboard().releaseAll();
}

void StrictPlayer::playback(const PlayRequest &request, quint64 session)
{
    const PanelConfig panel = config().panel();
    const KeyboardConfig keys = config().keyboard();
    TimelineOptions options;
    options.startMs = request.startSec * 1000.0;
    options.longPress = panel.longPressMode;
    options.delayNextMs = ConfigService::safeDelayNextMs(panel);
    const KeyResolver resolve = [&keys](const std::string &raw) { return resolveOutputKey(raw, keys); };

    const std::vector<PlaybackNote> notes = preparePlaybackNotes(request.notes);
    const std::vector<TimelineEvent> timeline = notes.empty() ? buildKeyMapTimeline(request.keyMap, options, resolve)
                                                              : buildNoteTimeline(notes, options, resolve);

    std::unordered_map<std::string, std::optional<skyinput::KeyCode>> codes;
    const auto codeFor = [&](const std::string &name) {
        auto it = codes.find(name);
        if (it == codes.end()) {
            it = codes.emplace(name, keyCodeFor(name)).first;
        }
        return it->second;
    };
    const SongClock::SpeedSource speed = [this] { return config().speed(); };
    const SongClock::ActiveCheck active = [this, session] { return isActive(session); };

    SongClock clock(options.startMs * kSyncCalibration);
    for (const TimelineEvent &event : timeline) {
        if (!clock.waitUntil(event.songTime, speed, active)) {
            releaseHeldKeys();
            return;
        }
        if (const auto code = codeFor(event.key)) {
            (void)keyboard().toggleKey(*code, event.press);
        }
    }
    preciseSleep(kTailSleepMs);
    releaseHeldKeys();
    finishNaturally(session);
}

void StrictPlayer::trackProgress(const PlayRequest &request, quint64 session)
{
    const int startSecond = request.startSec;
    const double totalDuration = request.keyMap.empty() ? startSecond * 1000.0 : request.keyMap.lastTime();
    const int maxSecond = static_cast<int>(std::trunc(totalDuration / 1000.0));
    const SongClock::SpeedSource speed = [this] { return config().speed(); };
    const SongClock::ActiveCheck active = [this, session] { return isActive(session); };

    SongClock clock(startSecond * 1000.0);
    for (int i = startSecond; i <= maxSecond; ++i) {
        if (!isActive(session)) {
            return;
        }
        emitProgress(session, i);
        if (i < maxSecond && !clock.waitUntil((i + 1) * 1000.0, speed, active)) {
            return;
        }
    }
}

}  // namespace sky
