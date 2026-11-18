#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/json.hpp"

namespace sky {

struct SongNote {
    std::int64_t time = 0;
    std::string key;
    std::int64_t hold = 0;

    bool operator==(const SongNote &) const = default;
};

struct PlaybackNote {
    std::int64_t time = 0;
    std::string key;
    std::int64_t hold = 0;

    bool operator==(const PlaybackNote &) const = default;
};

template <typename Note>
OrderedJson notesToJson(const std::vector<Note> &notes)
{
    OrderedJson array = OrderedJson::array();
    for (const Note &note : notes) {
        OrderedJson item = OrderedJson::object();
        item["time"] = note.time;
        item["key"] = note.key;
        if (note.hold > 0) {
            item["hold"] = note.hold;
        }
        array.push_back(std::move(item));
    }
    return array;
}

template <typename Note>
bool anyHoldNotes(const std::vector<Note> &notes)
{
    for (const Note &note : notes) {
        if (note.hold > 0) {
            return true;
        }
    }
    return false;
}

}  // namespace sky
