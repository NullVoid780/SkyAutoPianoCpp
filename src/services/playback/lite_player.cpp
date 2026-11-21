#include "services/playback/lite_player.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "core/precise_sleep.hpp"
#include "model/key_table.hpp"
#include "services/settings/config_service.hpp"
#include "services/playback/song_clock.hpp"
#include "services/playback/timeline_builder.hpp"

namespace sky {

LitePlayer::LitePlayer(ConfigService &config, skyinput::Keyboard &keyboard, QObject *parent)
    : PlayerBase(config, keyboard, parent), m_scheduler(keyboard)
{
}

LitePlayer::~LitePlayer()
{
    shutdown();
    releaseHeldKeys();
}

void LitePlayer::releaseHeldKeys()
{
    m_scheduler.releaseAllNow();
    keyboard().releaseAll();
}

bool LitePlayer::sleepFor(double milliseconds, quint64 session, bool ownsKeys)
{
    constexpr double kSliceMs = 25.0;
    double remaining = milliseconds;
    while (true) {
        if (!isActive(session)) {
            return false;
        }
        if (isHeld()) {
            if (ownsKeys) {
                releaseHeldKeys();
            }
            while (isHeld()) {
                if (!isActive(session)) {
                    return false;
                }
                preciseSleep(kHoldSliceMs);
            }
        }
        if (remaining <= 0) {
            return isActive(session);
        }
        const double slice = std::min(remaining, kSliceMs);
        const auto begin = std::chrono::steady_clock::now();
        preciseSleepWhile(slice, [this, session] { return isActive(session) && !isHeld(); });
        remaining -= std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    }
}

void LitePlayer::sendKey(const std::string &rawKey, std::optional<double> durationMs)
{
    const std::string outputKey = resolveOutputKey(rawKey, config().keyboard());
    const auto code = keyCodeFor(outputKey);
    if (!code) {
        return;
    }
    const double hold = durationMs.value_or(static_cast<double>(skyinput::kDefaultDelay.count()));
    (void)keyboard().toggleKey(*code, true);
    const auto releaseAt = ReleaseScheduler::Clock::now() +
                           std::chrono::duration_cast<ReleaseScheduler::Clock::duration>(
                               std::chrono::duration<double, std::milli>(std::max(0.0, hold)));
    m_scheduler.schedule(*code, releaseAt);
}

void LitePlayer::playback(const PlayRequest &request, quint64 session)
{
    const std::vector<PlaybackNote> notes = preparePlaybackNotes(request.notes);
    if (!notes.empty()) {
        playNotes(notes, request.startSec, session);
    } else {
        playKeyMap(request.keyMap, session);
    }
    if (isActive(session)) {
        finishNaturally(session);
    }
}

void LitePlayer::playNotes(const std::vector<PlaybackNote> &notes, int startSec, quint64 session)
{
    const PanelConfig startPanel = config().panel();
    const double startMs = startSec * 1000.0;
    const bool longPress = startPanel.longPressMode;
    const double speed = ConfigService::safeSpeed(startPanel);
    const std::map<std::int64_t, double> durations = stepDurations(notes, ConfigService::safeDelayNextMs(startPanel));

    std::vector<PlaybackNote> prepared;
    prepared.reserve(notes.size());
    for (const PlaybackNote &note : notes) {
        if (note.time < startMs) {
            if (!(longPress && note.hold > 0 && note.time + note.hold > startMs)) {
                continue;
            }
            const auto start = static_cast<std::int64_t>(startMs);
            prepared.push_back(PlaybackNote{start, note.key, note.time + note.hold - start});
            continue;
        }
        prepared.push_back(note);
    }

    double previous = startMs;
    for (const PlaybackNote &note : prepared) {
        if (!isActive(session)) {
            return;
        }
        const double wait = std::max(0.0, std::trunc((note.time - previous) / speed));
        if (wait > 0 && !sleepFor(wait, session)) {
            return;
        }

        std::optional<double> duration;
        if (longPress) {
            const PanelConfig live = config().panel();
            const double liveSpeed = ConfigService::safeSpeed(live);
            double base = static_cast<double>(note.hold);
            if (note.hold <= 0) {
                const auto found = durations.find(note.time);
                base = found != durations.end() ? found->second : ConfigService::safeDelayNextMs(live);
            }
            duration = std::max(kLiteMinPressMs, std::trunc(base / liveSpeed) - kLiteReleaseTrimMs);
        }
        sendKey(note.key, duration);
        previous = static_cast<double>(note.time);
    }
}

void LitePlayer::playKeyMap(const KeyMap &keyMap, quint64 session)
{
    const auto &entries = keyMap.entries();
    const PanelConfig startPanel = config().panel();
    const bool longPress = startPanel.longPressMode;
    const double delayNextMs = ConfigService::safeDelayNextMs(startPanel);

    for (std::size_t i = 1; i < entries.size(); ++i) {
        if (!isActive(session)) {
            return;
        }
        const double speed = config().speed();
        const double delay = std::trunc((entries[i].time - entries[i - 1].time) / speed);
        const double longPressDuration = entries[i].keys.empty() ? delayNextMs : 0.0;
        for (const std::string &key : entries[i - 1].keys) {
            std::optional<double> duration;
            if (longPress) {
                duration = (longPressDuration != 0.0 ? longPressDuration : delay) - kLiteReleaseTrimMs;
            }
            sendKey(key, duration);
        }
        if (!sleepFor(delay, session)) {
            return;
        }
    }

    if (entries.empty()) {
        return;
    }
    const PanelConfig endPanel = config().panel();
    for (const std::string &key : entries.back().keys) {
        std::optional<double> duration;
        if (endPanel.longPressMode) {
            duration = ConfigService::safeDelayNextMs(endPanel) - kLiteReleaseTrimMs;
        }
        sendKey(key, duration);
    }
}

void LitePlayer::trackProgress(const PlayRequest &request, quint64 session)
{
    const double totalDuration = request.keyMap.empty() ? 0.0 : request.keyMap.lastTime();
    const int lastSecond = static_cast<int>(std::trunc(totalDuration / 1000.0));
    for (int i = request.startSec; i <= lastSecond; ++i) {
        const double speed = config().speed();
        if (!sleepFor(std::trunc(1000.0 / speed), session, false)) {
            return;
        }
        emitProgress(session, i);
    }
}

}  // namespace sky
