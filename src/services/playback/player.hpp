#pragma once

#include <vector>

#include "model/key_map.hpp"
#include "model/note.hpp"

namespace sky {

struct PlayRequest {
    KeyMap keyMap;
    std::vector<PlaybackNote> notes;
    int startSec = 0;
};

class IPlayer {
public:
    virtual ~IPlayer() = default;

    virtual void start(const PlayRequest &request) = 0;
    virtual void stop(bool emitStopEvent, bool manual) = 0;
    virtual bool isPlaying() const = 0;
};

}  // namespace sky
