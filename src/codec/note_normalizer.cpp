#include "codec/note_normalizer.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/translator.hpp"
#include "core/string_utils.hpp"
#include "model/key_table.hpp"

namespace sky {

namespace {

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

std::string noteKeyText(const OrderedJson &note)
{
    if (!note.is_object()) {
        return {};
    }
    const auto key = note.find("key");
    if (key == note.end() || !key->is_string()) {
        return {};
    }
    return std::string(trimJs(key->get_ref<const std::string &>()));
}

}  // namespace

bool parseSongKey(std::string_view key, SongKeyParts &parts)
{
    std::size_t pos = 0;
    while (pos < key.size() && isDigit(key[pos])) {
        ++pos;
    }
    if (key.substr(pos, 3) != "Key") {
        return false;
    }
    const std::string_view digits = key.substr(pos + 3);
    if (digits.empty() || !std::all_of(digits.begin(), digits.end(), isDigit)) {
        return false;
    }
    parts.hasPagePrefix = pos > 0;
    parts.index = jsNumberFromString(digits);
    return true;
}

int resolveBitsPerPage(double value)
{
    if (std::isfinite(value) && value > 0 && value <= 128) {
        return static_cast<int>(std::trunc(value));
    }
    return 16;
}

int resolveCompatibleBitsPerPage(const OrderedJson &songNotes, double configuredValue)
{
    const int configured = resolveBitsPerPage(configuredValue);
    if (!songNotes.is_array()) {
        return configured;
    }
    double maxKeyIndex = -1;
    for (const OrderedJson &note : songNotes) {
        SongKeyParts parts;
        if (parseSongKey(noteKeyText(note), parts)) {
            maxKeyIndex = std::max(maxKeyIndex, parts.index);
        }
    }
    if (maxKeyIndex >= configured && maxKeyIndex <= 14) {
        return static_cast<int>(maxKeyIndex) + 1;
    }
    return configured;
}

Result<std::vector<SongNote>> normalizeSongNotes(const OrderedJson &songNotes, int bitsPerPage,
                                                 std::vector<SheetWarning> &warnings)
{
    if (!songNotes.is_array() || songNotes.empty()) {
        return sheetError(SheetErrorCode::NoNotes, trUtf8("The sheet does not contain any playable notes."));
    }
    if (songNotes.size() > kMaxSheetNoteCount) {
        return sheetError(SheetErrorCode::TooManyNotes,
                          fromQString(tr("The sheet contains %1 notes; the limit is %2.")
                                          .arg(songNotes.size())
                                          .arg(kMaxSheetNoteCount)));
    }

    std::vector<SongNote> notes;
    notes.reserve(songNotes.size());
    int unprefixedCount = 0;
    for (std::size_t index = 0; index < songNotes.size(); ++index) {
        const OrderedJson &note = songNotes[index];
        const std::string position = std::to_string(index + 1);

        const double time = jsMember(note, "time");
        if (!std::isfinite(time) || time < 0 || time > kMaxSheetDurationMs) {
            return sheetError(SheetErrorCode::InvalidNoteTime,
                              fromQString(tr("Note %1 has an invalid time value.").arg(toQString(position))));
        }

        const std::string rawKey = noteKeyText(note);
        SongKeyParts parts;
        if (!parseSongKey(rawKey, parts)) {
            return sheetError(SheetErrorCode::InvalidNoteKey,
                              fromQString(tr("Note %1 uses unsupported key “%2”.")
                                              .arg(toQString(position),
                                                   rawKey.empty() ? tr("(empty)") : toQString(rawKey))));
        }
        const double keyIndex = parts.index;
        if (std::trunc(keyIndex) != keyIndex || keyIndex < 0 || keyIndex >= bitsPerPage || keyIndex > 14) {
            return sheetError(SheetErrorCode::KeyOutOfRange,
                              fromQString(tr("Note %1 uses key %2, but supported keys are 0–14.")
                                              .arg(toQString(position), toQString(formatJsNumber(keyIndex)))));
        }
        if (!parts.hasPagePrefix) {
            ++unprefixedCount;
        }

        SongNote normalized;
        normalized.time = static_cast<std::int64_t>(std::trunc(time));
        normalized.key = songKeyName(static_cast<int>(keyIndex));
        const double hold = jsMember(note, "hold");
        if (std::isfinite(hold) && hold > 0) {
            normalized.hold = static_cast<std::int64_t>(std::trunc(hold));
        }
        notes.push_back(std::move(normalized));
    }

    if (unprefixedCount > 0) {
        warnings.push_back(SheetWarning{
            "UNPREFIXED_KEYS_NORMALIZED",
            std::to_string(unprefixedCount) + " unprefixed keys were interpreted as page-one keys.",
            unprefixedCount});
    }
    std::stable_sort(notes.begin(), notes.end(), [](const SongNote &a, const SongNote &b) {
        if (a.time != b.time) {
            return a.time < b.time;
        }
        return a.key < b.key;
    });
    return notes;
}

}  // namespace sky
