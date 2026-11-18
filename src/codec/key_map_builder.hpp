#pragma once

#include <vector>

#include "core/json.hpp"
#include "core/result.hpp"
#include "model/key_map.hpp"
#include "model/note.hpp"

namespace sky {

Result<KeyMap> buildKeyMap(const std::vector<PlaybackNote> &playbackNotes);
Result<KeyMap> normalizeStoredKeyMap(const OrderedJson &rawKeyMap);

}  // namespace sky
