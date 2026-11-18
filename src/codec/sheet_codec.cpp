#include "codec/sheet_codec.hpp"

#include <algorithm>
#include <cmath>

#include "codec/abc_sheet_parser.hpp"
#include "codec/encrypted_notes.hpp"
#include "codec/json_sheet_parser.hpp"
#include "codec/key_map_builder.hpp"
#include "codec/midi_sheet_parser.hpp"
#include "codec/note_normalizer.hpp"
#include "codec/playback_note_builder.hpp"
#include "codec/text_decoder.hpp"
#include "core/string_utils.hpp"

namespace sky {

namespace {

bool isSeparator(char c)
{
    return c == '/' || c == '\\';
}

std::string cleanText(const OrderedJson &sheet, const char *key)
{
    const auto it = sheet.find(key);
    if (it == sheet.end() || !it->is_string()) {
        return {};
    }
    return std::string(trimJs(it->get_ref<const std::string &>()));
}

const OrderedJson &memberOrNull(const OrderedJson &object, const char *key)
{
    static const OrderedJson kNull;
    if (!object.is_object()) {
        return kNull;
    }
    const auto it = object.find(key);
    return it == object.end() ? kNull : *it;
}

double normalizeBpm(double bpm, std::vector<SheetWarning> &warnings)
{
    if (std::isfinite(bpm) && bpm > 0 && bpm <= 2000) {
        return bpm;
    }
    warnings.push_back({"BPM_DEFAULTED", "Missing or invalid BPM was replaced with 120.", 0});
    return 120.0;
}

double normalizePitch(double pitch, std::vector<SheetWarning> &warnings)
{
    if (std::isfinite(pitch) && pitch >= -12 && pitch <= 12) {
        return pitch;
    }
    warnings.push_back({"PITCH_DEFAULTED", "Missing or invalid pitch was replaced with 0.", 0});
    return 0.0;
}

Result<ParsedSheet> prepare(const OrderedJson &raw, SheetFormat format, std::string_view fileName)
{
    if (!raw.is_object()) {
        return sheetError(SheetErrorCode::InvalidSheet, "The sheet must contain one song object.");
    }

    std::vector<SheetWarning> warnings;
    std::string name = cleanText(raw, "name");
    if (name.empty()) {
        name = pathStem(fileName, pathExtension(fileName));
    }
    if (name.empty()) {
        name = "Untitled";
    }
    std::string author = cleanText(raw, "author");
    if (author.empty()) {
        author = "Unknown";
    }
    std::string transcribedBy = cleanText(raw, "transcribedBy");
    if (transcribedBy.empty()) {
        transcribedBy = "Unknown";
    }
    const double bpm = normalizeBpm(jsMember(raw, "bpm"), warnings);
    const double pitchLevel = normalizePitch(jsMember(raw, "pitchLevel"), warnings);
    const OrderedJson &rawNotes = memberOrNull(raw, "songNotes");
    const int bitsPerPage = resolveCompatibleBitsPerPage(rawNotes, jsMember(raw, "bitsPerPage"));

    auto notes = normalizeSongNotes(rawNotes, bitsPerPage, warnings);
    if (!notes) {
        return notes.error();
    }
    auto playbackNotes = buildPlaybackNotes(notes.value(), bitsPerPage);
    if (!playbackNotes) {
        return playbackNotes.error();
    }
    auto keyMap = buildKeyMap(playbackNotes.value());
    if (!keyMap) {
        return keyMap.error();
    }

    std::int64_t durationMs = 0;
    for (const SongNote &note : notes.value()) {
        durationMs = std::max(durationMs, note.time + note.hold);
    }
    const auto composed = raw.find("isComposed");
    const bool isComposed = !(composed != raw.end() && composed->is_boolean() && !composed->get<bool>());
    const bool holds = anyHoldNotes(notes.value());

    ParsedSheet parsed;
    parsed.sheet = Sheet{name, author, transcribedBy, isComposed, bpm, bitsPerPage, pitchLevel, false, notes.value()};
    parsed.storage = SheetPayload{std::move(keyMap).value(), notes.value()};
    parsed.preview = SheetPreview{format, name, author, transcribedBy, bpm, bitsPerPage, pitchLevel,
                                  notes.value().size(), durationMs, holds};
    parsed.warnings = std::move(warnings);
    return parsed;
}

}  // namespace

std::string_view sheetFormatName(SheetFormat format)
{
    switch (format) {
    case SheetFormat::Json:
        return "json";
    case SheetFormat::Abc:
        return "abc";
    case SheetFormat::Midi:
        return "midi";
    }
    return "json";
}

std::string_view pathBaseName(std::string_view path)
{
    std::size_t end = path.size();
    while (end > 0 && isSeparator(path[end - 1])) {
        --end;
    }
    std::size_t start = end;
    while (start > 0 && !isSeparator(path[start - 1])) {
        --start;
    }
    if (start == 0 && end >= 2 && path[1] == ':' &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))) {
        start = 2;
    }
    return path.substr(start, end - start);
}

std::string pathExtension(std::string_view path)
{
    const std::string_view base = pathBaseName(path);
    const std::size_t dot = base.rfind('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    const std::string_view before = base.substr(0, dot);
    if (std::all_of(before.begin(), before.end(), [](char c) { return c == '.'; })) {
        return {};
    }
    return std::string(base.substr(dot));
}

std::string pathStem(std::string_view path, std::string_view extension)
{
    if (!extension.empty() && extension == path) {
        return {};
    }
    const std::string_view base = pathBaseName(path);
    if (!extension.empty() && base.size() > extension.size() && endsWith(base, extension)) {
        return std::string(base.substr(0, base.size() - extension.size()));
    }
    return std::string(base);
}

Result<ParsedSheet> parseSheet(std::span<const std::byte> content, std::string_view fileName,
                               std::optional<SheetFormat> forced)
{
    const std::string displayName = fileName.empty() ? std::string("Untitled") : std::string(fileName);
    const std::string extension = toLowerAscii(pathExtension(displayName));

    SheetFormat format = SheetFormat::Json;
    Result<OrderedJson> raw = OrderedJson();
    if (forced == SheetFormat::Midi || extension == ".mid" || extension == ".midi") {
        format = SheetFormat::Midi;
        raw = parseMidiSheet(content, pathStem(displayName, extension));
    } else {
        const std::string text = decodeSheetText(content);
        std::string_view normalized = trimJs(text);
        if (startsWith(normalized, "\xEF\xBB\xBF")) {
            normalized.remove_prefix(3);
        }
        if (normalized.empty()) {
            return sheetError(SheetErrorCode::EmptyFile, "The selected sheet file is empty.");
        }
        if (forced == SheetFormat::Abc || (!forced && looksLikeAbc(normalized))) {
            format = SheetFormat::Abc;
            raw = parseAbcSheet(normalized, pathStem(displayName, extension));
        } else {
            raw = parseJsonSheet(normalized);
        }
    }
    if (!raw) {
        return raw.error();
    }

    OrderedJson sheet = std::move(raw).value();
    if (hasEncryptedNotes(sheet)) {
        auto decoded = decodeEncryptedNotes(memberOrNull(sheet, "songNotes"));
        if (!decoded) {
            return sheetError(SheetErrorCode::DecryptFailed,
                              "The sheet is encrypted but could not be decrypted. The file may be corrupted.",
                              decoded.error().userMessage);
        }
        sheet["songNotes"] = std::move(decoded).value();
        sheet["isEncrypted"] = false;
    }
    return prepare(sheet, format, displayName);
}

Result<StoredSheet> loadStoredSheet(const OrderedJson &payload, std::optional<double> bitsPerPage)
{
    if (!payload.is_object()) {
        return sheetError(SheetErrorCode::InvalidStoredPayload, "The stored sheet payload is invalid.");
    }
    const double requested = bitsPerPage.value_or(0.0);
    const bool useRequested = requested != 0.0 && !std::isnan(requested);
    const int bits = resolveBitsPerPage(useRequested ? requested : jsMember(payload, "bitsPerPage"));

    const OrderedJson &keyMapField = memberOrNull(payload, "keyMap");
    const OrderedJson &rawKeyMap = (keyMapField.is_object() || keyMapField.is_array()) ? keyMapField : payload;

    StoredSheet stored;
    stored.bitsPerPage = bits;
    const OrderedJson &rawNotes = memberOrNull(payload, "songNotes");
    if (rawNotes.is_array()) {
        std::vector<SheetWarning> ignored;
        auto notes = normalizeSongNotes(rawNotes, bits, ignored);
        if (!notes) {
            return notes.error();
        }
        auto playbackNotes = buildPlaybackNotes(notes.value(), bits);
        if (!playbackNotes) {
            return playbackNotes.error();
        }
        stored.hasHoldNotes = anyHoldNotes(notes.value());
        stored.songNotes = std::move(notes).value();
        stored.playbackNotes = std::move(playbackNotes).value();
    }
    auto keyMap = normalizeStoredKeyMap(rawKeyMap);
    if (!keyMap) {
        return keyMap.error();
    }
    stored.keyMap = std::move(keyMap).value();
    return stored;
}

}  // namespace sky
