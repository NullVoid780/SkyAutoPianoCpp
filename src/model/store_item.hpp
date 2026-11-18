#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sky {

struct StoreCreator {
    std::string handle;
    std::string displayName;
};

struct StoreItem {
    std::string sourceType;
    std::string catalogKey;
    std::string id;
    std::string title;
    std::string author;
    std::string transcribedBy;
    std::int64_t bpm = 0;
    std::int64_t bitsPerPage = 0;
    std::int64_t pitchLevel = 0;
    std::int64_t durationMs = 0;
    std::int64_t noteCount = 0;
    std::int64_t playCount = 0;
    std::int64_t downloadCount = 0;
    std::string difficulty;
    std::int64_t priceAmount = 0;
    std::string currency;
    std::vector<std::string> tags;
    std::string originCountryCode;
    std::string accessMode;
    std::string protectionMode;
    bool isProtected = false;
    bool isDownloadable = true;
    std::string viewerActionState;
    std::optional<StoreCreator> creator;
    std::string coverUrl;
    std::string sourceUrl;
};

struct StorePage {
    std::vector<StoreItem> items;
    std::int64_t limit = 20;
    std::int64_t offset = 0;
    std::int64_t total = 0;
};

struct StoreAvailability {
    bool canPlay = true;
    bool canSave = true;
    std::string message;
};

struct StoreRuntimeNote {
    std::int64_t timeMs = 0;
    std::vector<std::string> keys;
};

struct StorePlayerPayload {
    std::string id;
    std::string title;
    std::string author;
    std::string transcribedBy;
    std::int64_t sheetBpm = 120;
    std::int64_t durationMs = 0;
    std::int64_t noteCount = 0;
    std::string accessMode;
    std::int64_t priceAmount = 0;
    std::string currency;
    std::string playerMode;
    std::string instrument;
    std::string pitch;
    std::int64_t playerBpm = 120;
    std::int64_t bitsPerPage = 16;
    std::int64_t pitchLevel = 0;
    std::vector<StoreRuntimeNote> notes;
    bool hasEntitlement = false;
    bool isOwner = false;
    bool canDownload = false;
    bool canOpenFull = false;
};

}  // namespace sky
