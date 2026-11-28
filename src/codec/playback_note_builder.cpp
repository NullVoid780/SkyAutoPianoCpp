#include "codec/playback_note_builder.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include "core/string_utils.hpp"
#include "core/translator.hpp"
#include "codec/note_normalizer.hpp"
#include "codec/sheet_error.hpp"
#include "model/key_table.hpp"

namespace sky {

Result<std::vector<PlaybackNote>> buildPlaybackNotes(const std::vector<SongNote> &songNotes, int bitsPerPage)
{
    std::vector<PlaybackNote> notes;
    std::unordered_map<std::string, std::size_t> byId;
    for (const SongNote &note : songNotes) {
        SongKeyParts parts;
        const bool valid = parseSongKey(note.key, parts) && std::trunc(parts.index) == parts.index &&
                           parts.index >= 0 && parts.index < bitsPerPage && parts.index <= 14;
        if (!valid) {
            return sheetError(SheetErrorCode::InvalidNoteKey,
                              fromQString(tr("Unsupported playback key “%1”.").arg(toQString(note.key))));
        }
        const std::string outputKey(kAutoKeys[static_cast<std::size_t>(parts.index)]);
        const std::string id = std::to_string(note.time) + ":" + outputKey;
        const auto existing = byId.find(id);
        if (existing != byId.end()) {
            PlaybackNote &kept = notes[existing->second];
            if (note.hold > 0 && note.hold > kept.hold) {
                kept.hold = note.hold;
            }
            continue;
        }
        byId.emplace(id, notes.size());
        notes.push_back(PlaybackNote{note.time, outputKey, note.hold});
    }
    std::stable_sort(notes.begin(), notes.end(), [](const PlaybackNote &a, const PlaybackNote &b) {
        if (a.time != b.time) {
            return a.time < b.time;
        }
        return autoKeyLess(a.key, b.key);
    });
    return notes;
}

}  // namespace sky
