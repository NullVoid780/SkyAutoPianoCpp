#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "model/key_map.hpp"
#include "model/note.hpp"

namespace sky {

inline constexpr double kSyncCalibration = 1.003;
inline constexpr double kMinPressTimeSong = 25.0;
inline constexpr double kReleaseTrimSong = 35.0;
inline constexpr double kChordDelayStep = 3.0;
inline constexpr double kTailSleepMs = 50.0;

struct TimelineEvent {
    double songTime = 0.0;
    std::string key;
    bool press = true;

    bool operator==(const TimelineEvent &) const = default;
};

struct TimelineOptions {
    double startMs = 0.0;
    bool longPress = false;
    double delayNextMs = 0.0;
};

using KeyResolver = std::function<std::string(const std::string &)>;

std::vector<PlaybackNote> preparePlaybackNotes(const std::vector<PlaybackNote> &notes);
std::map<std::int64_t, double> stepDurations(const std::vector<PlaybackNote> &notes, double delayNextMs);
std::vector<TimelineEvent> buildNoteTimeline(const std::vector<PlaybackNote> &notes, const TimelineOptions &options,
                                             const KeyResolver &resolve);
std::vector<TimelineEvent> buildKeyMapTimeline(const KeyMap &keyMap, const TimelineOptions &options,
                                               const KeyResolver &resolve);
void sortTimeline(std::vector<TimelineEvent> &timeline);

}  // namespace sky
