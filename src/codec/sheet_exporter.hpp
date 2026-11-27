#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/json.hpp"
#include "core/result.hpp"
#include "model/key_map.hpp"
#include "model/note.hpp"
#include "model/sheet_entry.hpp"
#include "model/stored_sheet.hpp"

namespace sky {

enum class ExportLayout { Compact, Pretty };

ExportLayout exportLayoutForPath(std::string_view filePath);
std::vector<SongNote> songNotesFromKeyMap(const KeyMap &keyMap);
Result<SheetPayload> payloadFromSongNotes(std::vector<SongNote> songNotes);
OrderedJson buildExportDocument(const SheetEntry &entry, const std::vector<SongNote> &payloadNotes,
                                const KeyMap &keyMap);
std::string serializeExport(const OrderedJson &document, ExportLayout layout);

}  // namespace sky
