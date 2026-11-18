#include "services/playback/timeline_builder.hpp"

#include <algorithm>
#include <set>

#include "core/string_utils.hpp"
#include "model/key_table.hpp"

namespace sky {

std::vector<PlaybackNote> preparePlaybackNotes(const std::vector<PlaybackNote> &notes)
{
    std::vector<PlaybackNote> prepared;
    prepared.reserve(notes.size());
    for (const PlaybackNote &note : notes) {
        const std::string key(trimJs(note.key));
        if (note.time < 0 || key.empty()) {
            continue;
        }
        prepared.push_back(PlaybackNote{note.time, key, note.hold > 0 ? note.hold : 0});
    }
    std::stable_sort(prepared.begin(), prepared.end(), [](const PlaybackNote &a, const PlaybackNote &b) {
        if (a.time != b.time) {
            return a.time < b.time;
        }
        return autoKeyLess(a.key, b.key);
    });
    return prepared;
}

std::map<std::int64_t, double> stepDurations(const std::vector<PlaybackNote> &notes, double delayNextMs)
{
    std::set<std::int64_t> steps;
    for (const PlaybackNote &note : notes) {
        steps.insert(note.time);
    }
    std::map<std::int64_t, double> durations;
    for (auto it = steps.begin(); it != steps.end(); ++it) {
        const auto next = std::next(it);
        durations[*it] = next != steps.end() && *next != 0 ? static_cast<double>(*next - *it) : delayNextMs;
    }
    return durations;
}

std::vector<TimelineEvent> buildNoteTimeline(const std::vector<PlaybackNote> &notes, const TimelineOptions &options,
                                             const KeyResolver &resolve)
{
    const std::map<std::int64_t, double> durations = stepDurations(notes, options.delayNextMs);
    std::map<double, double> chordOffsets;
    std::vector<TimelineEvent> timeline;
    timeline.reserve(notes.size() * 2);

    for (const PlaybackNote &note : notes) {
        const std::string outputKey = resolve(note.key);
        if (outputKey.empty()) {
            continue;
        }
        double start = static_cast<double>(note.time);
        double hold = static_cast<double>(note.hold);
        if (start < options.startMs) {
            if (!(options.longPress && hold > 0 && start + hold > options.startMs)) {
                continue;
            }
            hold = start + hold - options.startMs;
            start = options.startMs;
        }

        double press = kMinPressTimeSong;
        if (options.longPress) {
            double base = hold;
            if (hold <= 0) {
                const auto found = durations.find(note.time);
                base = found != durations.end() ? found->second : options.delayNextMs;
            }
            press = std::max(kMinPressTimeSong, base * kSyncCalibration - kReleaseTrimSong);
        }

        double &chord = chordOffsets[start];
        const double at = start * kSyncCalibration + chord;
        chord += kChordDelayStep;
        timeline.push_back(TimelineEvent{at, outputKey, true});
        timeline.push_back(TimelineEvent{at + press, outputKey, false});
    }
    sortTimeline(timeline);
    return timeline;
}

std::vector<TimelineEvent> buildKeyMapTimeline(const KeyMap &keyMap, const TimelineOptions &options,
                                               const KeyResolver &resolve)
{
    std::vector<TimelineEvent> timeline;
    const auto &entries = keyMap.entries();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const double step = entries[i].time;
        if (step < options.startMs) {
            continue;
        }
        const double target = step * kSyncCalibration;
        const double longPressSong =
            i + 1 < entries.size() ? (entries[i + 1].time - step) * kSyncCalibration : options.delayNextMs;
        double chord = 0.0;
        for (const std::string &key : entries[i].keys) {
            const double press = options.longPress ? std::max(kMinPressTimeSong, longPressSong - kReleaseTrimSong)
                                                   : kMinPressTimeSong;
            const std::string outputKey = resolve(key);
            timeline.push_back(TimelineEvent{target + chord, outputKey, true});
            timeline.push_back(TimelineEvent{target + chord + press, outputKey, false});
            chord += kChordDelayStep;
        }
    }
    sortTimeline(timeline);
    return timeline;
}

void sortTimeline(std::vector<TimelineEvent> &timeline)
{
    std::stable_sort(timeline.begin(), timeline.end(), [](const TimelineEvent &a, const TimelineEvent &b) {
        if (a.songTime == b.songTime) {
            return !a.press && b.press;
        }
        return a.songTime < b.songTime;
    });
}

}  // namespace sky
