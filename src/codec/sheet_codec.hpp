#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "codec/sheet_error.hpp"
#include "core/json.hpp"
#include "core/result.hpp"
#include "model/sheet.hpp"
#include "model/stored_sheet.hpp"

namespace sky {

enum class SheetFormat { Json, Abc, Midi };

std::string_view sheetFormatName(SheetFormat format);

struct SheetPreview {
    SheetFormat format = SheetFormat::Json;
    std::string name;
    std::string author;
    std::string transcribedBy;
    double bpm = 120.0;
    int bitsPerPage = 16;
    double pitchLevel = 0.0;
    std::size_t noteCount = 0;
    std::int64_t durationMs = 0;
    bool hasHoldNotes = false;
};

struct ParsedSheet {
    Sheet sheet;
    SheetPayload storage;
    SheetPreview preview;
    std::vector<SheetWarning> warnings;
};

Result<ParsedSheet> parseSheet(std::span<const std::byte> content, std::string_view fileName,
                               std::optional<SheetFormat> forced = std::nullopt);

Result<StoredSheet> loadStoredSheet(const OrderedJson &payload,
                                    std::optional<double> bitsPerPage = std::nullopt);

std::string_view pathBaseName(std::string_view path);
std::string pathExtension(std::string_view path);
std::string pathStem(std::string_view path, std::string_view extension);

}  // namespace sky
