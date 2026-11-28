#include "services/store/store_mapping.hpp"

#include <cctype>
#include <cmath>
#include <initializer_list>
#include <set>

#include "core/translator.hpp"
#include "codec/key_map_builder.hpp"
#include "codec/note_normalizer.hpp"
#include "codec/playback_note_builder.hpp"
#include "core/string_utils.hpp"
#include "net/url.hpp"

namespace sky {

namespace {

constexpr const char *kInvalidDownload = trNoop("The downloaded sheet is invalid.");
constexpr std::int64_t kMaxRemoteDurationMs = 15 * 60 * 1000;
constexpr std::int64_t kMaxRemoteNoteCount = 20000;
constexpr std::size_t kMaxTitleLength = 180;

const OrderedJson &nullJson()
{
    static const OrderedJson kUndefined(OrderedJson::value_t::discarded);
    return kUndefined;
}

const OrderedJson &member(const OrderedJson &object, const char *key)
{
    if (!object.is_object()) {
        return nullJson();
    }
    const auto it = object.find(key);
    return it == object.end() ? nullJson() : *it;
}

const OrderedJson &firstTruthy(const OrderedJson &object, std::initializer_list<const char *> keys)
{
    const OrderedJson *last = &nullJson();
    for (const char *key : keys) {
        const OrderedJson &value = member(object, key);
        if (jsTruthy(value)) {
            return value;
        }
        last = &value;
    }
    return *last;
}

std::string trimmedString(const OrderedJson &value)
{
    return value.is_string() ? std::string(trimJs(value.get_ref<const std::string &>())) : std::string();
}

std::string orDefault(std::string value, const char *fallback)
{
    return value.empty() ? std::string(fallback) : value;
}

bool isObjectLike(const OrderedJson &value)
{
    return value.is_object() || value.is_array();
}

bool isHex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

Error storeError(const char *message)
{
    return Error{"STORE_INVALID_RESPONSE", trUtf8(message), {}};
}

Error invalidDownload()
{
    return Error{"STORE_INVALID_SHEET", trUtf8(kInvalidDownload), {}};
}

}  // namespace

bool isStoreSheetId(std::string_view id)
{
    static constexpr std::size_t kGroups[] = {8, 4, 4, 4, 12};
    if (id.size() != 36) {
        return false;
    }
    std::size_t pos = 0;
    for (std::size_t group = 0; group < 5; ++group) {
        for (std::size_t i = 0; i < kGroups[group]; ++i, ++pos) {
            if (!isHex(id[pos])) {
                return false;
            }
        }
        if (group < 4 && id[pos++] != '-') {
            return false;
        }
    }
    const char version = id[14];
    const char variant = static_cast<char>(std::tolower(static_cast<unsigned char>(id[19])));
    return version >= '1' && version <= '5' &&
           (variant == '8' || variant == '9' || variant == 'a' || variant == 'b');
}

std::int64_t clampInteger(const OrderedJson &value, std::int64_t min, std::int64_t max, std::int64_t fallback)
{
    const double number = value.is_discarded() ? kNaN : jsNumber(value);
    if (!std::isfinite(number)) {
        return fallback;
    }
    const double truncated = std::trunc(number);
    if (truncated < static_cast<double>(min) || truncated > static_cast<double>(max)) {
        return fallback;
    }
    return static_cast<std::int64_t>(truncated);
}

Result<StoreItem> mapStoreSummary(const OrderedJson &item)
{
    StoreItem mapped;
    mapped.id = trimmedString(member(item, "id"));
    if (!isStoreSheetId(mapped.id)) {
        return storeError(trNoop("Sky Sheet Store returned an invalid sheet id."));
    }
    const auto numberOr = [&item](std::initializer_list<const char *> keys, std::int64_t min, std::int64_t max) {
        return clampInteger(firstTruthy(item, keys), min, max, 0);
    };

    const std::string rawSourceType = trimmedString(firstTruthy(item, {"sourceType", "source_type"}));
    mapped.sourceType = orDefault(rawSourceType, "user");
    mapped.catalogKey = trimmedString(firstTruthy(item, {"catalogKey", "catalog_key"}));
    if (mapped.catalogKey.empty()) {
        mapped.catalogKey = "user:" + mapped.id;
    }
    mapped.title = orDefault(trimmedString(member(item, "title")), "Unknown");
    mapped.author = orDefault(trimmedString(firstTruthy(item, {"author", "artistName", "artist_name"})), "Unknown");
    mapped.transcribedBy = orDefault(
        trimmedString(firstTruthy(item, {"transcribed_by", "transcribedBy", "composerName", "composer_name"})),
        "Unknown");
    mapped.bpm = clampInteger(member(item, "bpm"), 0, 2000, 0);
    mapped.bitsPerPage = numberOr({"bits_per_page", "bitsPerPage"}, 0, 128);
    mapped.pitchLevel = numberOr({"pitch_level", "pitchLevel"}, -12, 12);
    mapped.durationMs = numberOr({"duration_ms", "durationMs"}, 0, kMaxRemoteDurationMs);
    mapped.noteCount = numberOr({"note_count", "noteCount"}, 0, kMaxRemoteNoteCount);
    mapped.playCount = numberOr({"play_count", "playCount", "playerOpenCount", "player_open_count"}, 0, kMaxSafeInteger);
    mapped.downloadCount = numberOr({"download_count", "downloadCount"}, 0, kMaxSafeInteger);
    mapped.difficulty = orDefault(trimmedString(member(item, "difficulty")), "Unknown");
    mapped.priceAmount = numberOr({"price_amount", "priceAmount"}, 0, kMaxSafeInteger);
    mapped.currency = orDefault(trimmedString(member(item, "currency")), "USD");
    const OrderedJson &tags = member(item, "tags");
    if (tags.is_array()) {
        for (const OrderedJson &tag : tags) {
            if (std::string value = trimmedString(tag); !value.empty()) {
                mapped.tags.push_back(std::move(value));
            }
        }
    }
    mapped.originCountryCode = trimmedString(member(item, "originCountryCode"));
    mapped.accessMode = orDefault(trimmedString(firstTruthy(item, {"accessMode", "access_mode"})), "downloadable");
    mapped.protectionMode = orDefault(trimmedString(member(item, "protectionMode")), "none");
    mapped.isProtected = jsTruthy(member(item, "isProtected"));
    if (item.is_object() && item.contains("isDownloadable")) {
        mapped.isDownloadable = jsTruthy(item["isDownloadable"]);
    } else if (item.is_object() && item.contains("is_downloadable")) {
        mapped.isDownloadable = jsTruthy(item["is_downloadable"]);
    } else {
        mapped.isDownloadable = true;
    }
    mapped.viewerActionState = trimmedString(member(item, "viewerActionState"));
    const OrderedJson &creator = member(item, "creator");
    if (isObjectLike(creator)) {
        mapped.creator = StoreCreator{trimmedString(member(creator, "handle")),
                                      orDefault(trimmedString(member(creator, "displayName")), "Unknown")};
    }
    mapped.coverUrl = normalizeAllowedUrl(trimmedString(firstTruthy(item, {"coverUrl", "cover_url", "coverImageUrl"})),
                                          kStoreApiBase, kStoreApiHost);
    mapped.sourceUrl = std::string(kStoreSiteBase) + (rawSourceType == "archive" ? "/sheets/archive/" : "/sheets/") +
                       mapped.id;
    return mapped;
}

Result<StorePage> mapStorePage(const OrderedJson &payload, std::int64_t limit, std::int64_t offset)
{
    const OrderedJson &items = member(payload, "items");
    if (!items.is_array()) {
        return storeError(trNoop("Sky Sheet Store returned an invalid sheet list."));
    }
    StorePage page;
    for (const OrderedJson &item : items) {
        auto mapped = mapStoreSummary(item);
        if (!mapped) {
            return mapped.error();
        }
        page.items.push_back(std::move(mapped).value());
    }
    page.limit = clampInteger(member(payload, "limit"), 1, 100, limit);
    page.offset = clampInteger(member(payload, "offset"), 0, kMaxSafeInteger, offset);
    page.total = clampInteger(member(payload, "total"), 0, kMaxSafeInteger, 0);
    return page;
}

Result<StorePlayerPayload> mapStorePlayerPayload(const OrderedJson &payload)
{
    const OrderedJson &sheet = member(payload, "sheet");
    const OrderedJson &player = member(payload, "player");
    const OrderedJson &runtime = member(payload, "runtime");
    const OrderedJson &viewer = member(payload, "viewer");
    if (!isObjectLike(sheet) || !isObjectLike(player) || !isObjectLike(runtime) ||
        !member(runtime, "notes").is_array()) {
        return storeError(trNoop("Sky Sheet Store returned an incomplete player payload."));
    }
    StorePlayerPayload mapped;
    mapped.id = trimmedString(member(sheet, "id"));
    if (!isStoreSheetId(mapped.id)) {
        return storeError(trNoop("Sky Sheet Store returned an invalid player sheet id."));
    }
    mapped.title = orDefault(trimmedString(member(sheet, "title")), "Unknown");
    mapped.author = trimmedString(member(sheet, "author"));
    mapped.transcribedBy = trimmedString(member(sheet, "transcribedBy"));
    mapped.sheetBpm = clampInteger(member(sheet, "bpm"), 1, 2000, 120);
    mapped.durationMs = clampInteger(member(sheet, "durationMs"), 0, kMaxRemoteDurationMs, 0);
    mapped.noteCount = clampInteger(member(sheet, "noteCount"), 0, kMaxRemoteNoteCount, 0);
    mapped.accessMode = orDefault(trimmedString(member(sheet, "accessMode")), "downloadable");
    mapped.priceAmount = clampInteger(member(sheet, "priceAmount"), 0, kMaxSafeInteger, 0);
    mapped.currency = orDefault(trimmedString(member(sheet, "currency")), "USD");
    mapped.playerMode = orDefault(trimmedString(member(player, "mode")), "full");
    mapped.instrument = trimmedString(member(player, "instrument"));
    mapped.pitch = trimmedString(member(player, "pitch"));
    mapped.playerBpm = clampInteger(member(player, "bpm"), 1, 2000, 120);
    mapped.bitsPerPage = clampInteger(member(player, "bitsPerPage"), 1, 128, 16);
    mapped.pitchLevel = clampInteger(member(player, "pitchLevel"), -12, 12, 0);
    for (const OrderedJson &note : member(runtime, "notes")) {
        StoreRuntimeNote mappedNote;
        mappedNote.timeMs = clampInteger(member(note, "timeMs"), 0, kMaxRemoteDurationMs, 0);
        const OrderedJson &keys = member(note, "keys");
        if (keys.is_array()) {
            for (const OrderedJson &key : keys) {
                if (std::string value = trimmedString(key); !value.empty()) {
                    mappedNote.keys.push_back(std::move(value));
                }
            }
        }
        mapped.notes.push_back(std::move(mappedNote));
    }
    mapped.hasEntitlement = jsTruthy(member(viewer, "hasEntitlement"));
    mapped.isOwner = jsTruthy(member(viewer, "isOwner"));
    mapped.canDownload = jsTruthy(member(viewer, "canDownload"));
    mapped.canOpenFull = jsTruthy(member(viewer, "canOpenFull"));
    return mapped;
}

StoreAvailability storeAvailability(const StoreItem &item)
{
    if (item.sourceType == "archive") {
        return {true, true, {}};
    }
    if (item.accessMode == "web_only") {
        return {false, false, trUtf8("Web only")};
    }
    if (!item.isDownloadable) {
        return {false, false, trUtf8("Not downloadable")};
    }
    if (item.priceAmount > 0 || item.viewerActionState == "checkout") {
        return {false, false, trUtf8("Purchase required")};
    }
    if (item.viewerActionState == "login_to_open") {
        return {false, false, trUtf8("Login required")};
    }
    return {true, true, {}};
}

Result<OrderedJson> normalizeStorePlayerSheet(const StorePlayerPayload &payload, const StoreItem &item)
{
    const std::string title(trimJs(!payload.title.empty() ? payload.title : item.title));
    if (title.empty() || utf16Length(title) > kMaxTitleLength) {
        return invalidDownload();
    }
    const auto bpm = static_cast<double>(payload.playerBpm != 0 ? payload.playerBpm : payload.sheetBpm);
    const auto bitsPerPage = static_cast<double>(payload.bitsPerPage);
    const auto pitchLevel = static_cast<double>(payload.pitchLevel);
    if (bpm <= 0 || bpm > 2000 || bitsPerPage <= 0 || bitsPerPage > 128 || pitchLevel < -12 || pitchLevel > 12) {
        return invalidDownload();
    }
    if (payload.notes.empty()) {
        return invalidDownload();
    }

    const int bits = resolveBitsPerPage(bitsPerPage);
    OrderedJson songNotes = OrderedJson::array();
    std::set<std::string> seen;
    std::int64_t previous = -1;
    for (const StoreRuntimeNote &group : payload.notes) {
        if (group.timeMs < 0 || group.timeMs > kMaxRemoteDurationMs || group.timeMs < previous) {
            return invalidDownload();
        }
        previous = group.timeMs;
        for (const std::string &key : group.keys) {
            SongKeyParts parts;
            if (!parseSongKey(std::string(trimJs(key)), parts) || std::trunc(parts.index) != parts.index ||
                parts.index < 0 || parts.index >= bits || parts.index > 14) {
                return invalidDownload();
            }
            if (!seen.insert(std::to_string(group.timeMs) + ":" + key).second) {
                continue;
            }
            OrderedJson note = OrderedJson::object();
            note["time"] = group.timeMs;
            note["key"] = key;
            songNotes.push_back(std::move(note));
            if (static_cast<std::int64_t>(songNotes.size()) > kMaxRemoteNoteCount) {
                return invalidDownload();
            }
        }
    }
    if (songNotes.empty()) {
        return invalidDownload();
    }

    const auto fallbackText = [](const std::string &primary, const std::string &secondary) {
        const std::string chosen = !primary.empty() ? primary : (!secondary.empty() ? secondary : "Unknown");
        const std::string trimmed(trimJs(chosen));
        return trimmed.empty() ? std::string("Unknown") : trimmed;
    };

    OrderedJson sheet = OrderedJson::object();
    sheet["name"] = title;
    sheet["author"] = fallbackText(payload.author, item.author);
    sheet["transcribedBy"] = fallbackText(payload.transcribedBy, item.transcribedBy);
    sheet["isComposed"] = true;
    sheet["bpm"] = jsonNumber(bpm);
    sheet["bitsPerPage"] = jsonNumber(bitsPerPage);
    sheet["pitchLevel"] = jsonNumber(pitchLevel);
    sheet["isEncrypted"] = false;
    sheet["songNotes"] = std::move(songNotes);
    sheet["hasHoldNotes"] = false;
    return sheet;
}

Result<PreparedStoreSheet> prepareStoreSheet(const StorePlayerPayload &payload, const StoreItem &item)
{
    auto normalized = normalizeStorePlayerSheet(payload, item);
    if (!normalized) {
        return normalized.error();
    }
    const OrderedJson &sheet = normalized.value();
    std::vector<SongNote> songNotes;
    for (const OrderedJson &note : sheet["songNotes"]) {
        songNotes.push_back(SongNote{note["time"].get<std::int64_t>(), note["key"].get<std::string>(), 0});
    }
    const int bits = resolveBitsPerPage(sheet["bitsPerPage"].get<double>());
    auto playbackNotes = buildPlaybackNotes(songNotes, bits);
    if (!playbackNotes) {
        return Error{"STORE_INVALID_SHEET", trUtf8("The sheet file is not valid, please try again with another file!"), {}};
    }
    auto keyMap = buildKeyMap(playbackNotes.value());
    if (!keyMap) {
        return Error{"STORE_INVALID_SHEET", trUtf8("The sheet file is not valid, please try again with another file!"), {}};
    }
    PreparedStoreSheet prepared;
    prepared.normalizedSheet = sheet;
    prepared.keyMap = std::move(keyMap).value();
    prepared.hasHoldNotes = false;
    return prepared;
}

}  // namespace sky
