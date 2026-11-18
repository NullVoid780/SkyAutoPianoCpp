#pragma once

#include <vector>

#include "core/result.hpp"
#include "model/note.hpp"

namespace sky {

Result<std::vector<PlaybackNote>> buildPlaybackNotes(const std::vector<SongNote> &songNotes, int bitsPerPage);

}  // namespace sky
