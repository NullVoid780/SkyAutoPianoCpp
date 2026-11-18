#include "codec/sheet_exporter.hpp"

#include <algorithm>

#include "core/string_utils.hpp"
#include "model/key_table.hpp"

namespace sky {

namespace {

OrderedJson fieldOr(const SheetEntry &entry, const char *key, OrderedJson fallback)
{
    const OrderedJson *value = entry.field(key);
    return (value && jsTruthy(*value)) ? *value : fallback;
}

}  // namespace

ExportLayout exportLayoutForPath(std::string_view filePath)
{
    const std::size_t dot = filePath.rfind('.');
    const std::string extension = toLowerAscii(dot == std::string_view::npos ? filePath : filePath.substr(dot + 1));
    return extension == "txt" ? ExportLayout::Compact : ExportLayout::Pretty;
}

std::vector<SongNote> songNotesFromKeyMap(const KeyMap &keyMap)
{
    std::vector<SongNote> notes;
    for (const KeyMapEntry &entry : keyMap.entries()) {
        for (const std::string &key : entry.keys) {
            const int index = autoKeyIndex(key);
            if (index < 0) {
                continue;
            }
            notes.push_back(SongNote{static_cast<std::int64_t>(entry.time), songKeyName(index), 0});
        }
    }
    std::stable_sort(notes.begin(), notes.end(), [](const SongNote &a, const SongNote &b) {
        if (a.time != b.time) {
            return a.time < b.time;
        }
        return a.key < b.key;
    });
    return notes;
}

OrderedJson buildExportDocument(const SheetEntry &entry, const std::vector<SongNote> &payloadNotes,
                                const KeyMap &keyMap)
{
    OrderedJson sheet = OrderedJson::object();
    sheet["name"] = fieldOr(entry, "name", "Untitled");
    sheet["author"] = fieldOr(entry, "author", "Unknown");
    sheet["transcribedBy"] = fieldOr(entry, "transcribedBy", "Unknown");
    const OrderedJson *composed = entry.field("isComposed");
    sheet["isComposed"] = composed && composed->is_boolean() ? *composed : OrderedJson(true);
    sheet["bpm"] = fieldOr(entry, "bpm", 120);
    if (const OrderedJson *bits = entry.field("bitsPerPage")) {
        sheet["bitsPerPage"] = *bits;
    }
    if (const OrderedJson *pitch = entry.field("pitchLevel")) {
        sheet["pitchLevel"] = *pitch;
    }
    sheet["isEncrypted"] = false;
    sheet["songNotes"] = notesToJson(payloadNotes.empty() ? songNotesFromKeyMap(keyMap) : payloadNotes);
    return OrderedJson::array({sheet});
}

std::string serializeExport(const OrderedJson &document, ExportLayout layout)
{
    return dumpJson(document, layout == ExportLayout::Compact ? -1 : 4);
}

}  // namespace sky
