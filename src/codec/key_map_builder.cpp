#include "codec/key_map_builder.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>

#include "codec/sheet_error.hpp"
#include "core/string_utils.hpp"
#include "model/key_table.hpp"

namespace sky {

Result<KeyMap> buildKeyMap(const std::vector<PlaybackNote> &playbackNotes)
{
    std::map<std::int64_t, std::vector<std::string>> byTime;
    std::int64_t maxSongTime = 0;
    for (const PlaybackNote &note : playbackNotes) {
        auto &keys = byTime[note.time];
        if (std::find(keys.begin(), keys.end(), note.key) == keys.end()) {
            keys.push_back(note.key);
        }
        maxSongTime = std::max(maxSongTime, note.time + note.hold);
    }
    if (byTime.empty()) {
        return sheetError(SheetErrorCode::NoNotes, "The sheet does not contain any playable notes.");
    }
    const std::int64_t lastTimestamp = byTime.rbegin()->first;
    byTime.try_emplace(0);
    const std::int64_t terminal = (std::max(maxSongTime, lastTimestamp) / 1000 + 1) * 1000;
    byTime[terminal].clear();

    std::vector<KeyMapEntry> entries;
    entries.reserve(byTime.size());
    for (auto &[time, keys] : byTime) {
        entries.push_back(KeyMapEntry{static_cast<double>(time), std::move(keys)});
    }
    return KeyMap(std::move(entries));
}

Result<KeyMap> normalizeStoredKeyMap(const OrderedJson &rawKeyMap)
{
    if (!rawKeyMap.is_object()) {
        return sheetError(SheetErrorCode::InvalidKeyMap, "The stored sheet keymap is invalid.");
    }
    std::set<double> times;
    for (const auto &item : rawKeyMap.items()) {
        const double time = jsNumberFromString(item.key());
        if (std::isfinite(time) && time >= 0) {
            times.insert(time);
        }
    }

    std::vector<KeyMapEntry> entries;
    entries.reserve(times.size());
    for (double time : times) {
        KeyMapEntry entry{time, {}};
        const auto found = rawKeyMap.find(formatJsNumber(time));
        if (found != rawKeyMap.end() && found->is_array()) {
            for (const OrderedJson &key : *found) {
                const std::string safeKey = key.is_string() ? std::string(trimJs(key.get_ref<const std::string &>())) : std::string();
                if (autoKeyIndex(safeKey) < 0 ||
                    std::find(entry.keys.begin(), entry.keys.end(), safeKey) != entry.keys.end()) {
                    continue;
                }
                entry.keys.push_back(safeKey);
            }
        }
        entries.push_back(std::move(entry));
    }
    return KeyMap(std::move(entries));
}

}  // namespace sky
