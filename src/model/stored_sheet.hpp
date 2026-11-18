#pragma once

#include <optional>
#include <vector>

#include "model/key_map.hpp"
#include "model/note.hpp"

namespace sky {

struct SheetPayload {
    KeyMap keyMap;
    std::vector<SongNote> songNotes;

    OrderedJson toJson() const
    {
        OrderedJson payload = OrderedJson::object();
        payload["version"] = 2;
        payload["keyMap"] = keyMap.toJson();
        payload["songNotes"] = notesToJson(songNotes);
        return payload;
    }
};

struct StoredSheet {
    KeyMap keyMap;
    std::optional<std::vector<SongNote>> songNotes;
    std::optional<std::vector<PlaybackNote>> playbackNotes;
    bool hasHoldNotes = false;
    int bitsPerPage = 16;

    SheetPayload toPayload() const
    {
        return SheetPayload{keyMap, songNotes.value_or(std::vector<SongNote>{})};
    }
};

}  // namespace sky
