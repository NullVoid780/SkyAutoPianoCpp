#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/json.hpp"
#include "core/result.hpp"
#include "model/key_map.hpp"
#include "model/note.hpp"
#include "model/store_item.hpp"

namespace sky {

inline constexpr const char *kStoreApiBase = "https://api.skysheet.store";
inline constexpr const char *kStoreSiteBase = "https://skysheet.store";
inline constexpr const char *kStoreApiHost = "api.skysheet.store";
inline constexpr std::int64_t kMaxSafeInteger = 9007199254740991LL;

struct PreparedStoreSheet {
    OrderedJson normalizedSheet;
    KeyMap keyMap;
    std::vector<PlaybackNote> playbackNotes;
    bool hasHoldNotes = false;
};

bool isStoreSheetId(std::string_view id);
std::int64_t clampInteger(const OrderedJson &value, std::int64_t min, std::int64_t max, std::int64_t fallback);
Result<StoreItem> mapStoreSummary(const OrderedJson &item);
Result<StorePage> mapStorePage(const OrderedJson &payload, std::int64_t limit, std::int64_t offset);
Result<StorePlayerPayload> mapStorePlayerPayload(const OrderedJson &payload);
StoreAvailability storeAvailability(const StoreItem &item);
Result<OrderedJson> normalizeStorePlayerSheet(const StorePlayerPayload &payload, const StoreItem &item);
Result<PreparedStoreSheet> prepareStoreSheet(const StorePlayerPayload &payload, const StoreItem &item);

}  // namespace sky
