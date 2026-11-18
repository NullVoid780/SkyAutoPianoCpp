#pragma once

#include <vector>

#include "codec/sheet_error.hpp"
#include "core/json.hpp"
#include "core/result.hpp"
#include "model/note.hpp"

namespace sky {

inline constexpr double kMaxSheetDurationMs = 15.0 * 60.0 * 1000.0;
inline constexpr std::size_t kMaxSheetNoteCount = 20000;

struct SongKeyParts {
    bool hasPagePrefix = false;
    double index = 0.0;
};

bool parseSongKey(std::string_view key, SongKeyParts &parts);
int resolveBitsPerPage(double value);
int resolveCompatibleBitsPerPage(const OrderedJson &songNotes, double configured);
Result<std::vector<SongNote>> normalizeSongNotes(const OrderedJson &songNotes, int bitsPerPage,
                                                 std::vector<SheetWarning> &warnings);

}  // namespace sky
