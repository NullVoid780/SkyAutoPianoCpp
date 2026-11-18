#pragma once

#include <string>
#include <string_view>

#include "core/error.hpp"

namespace sky {

enum class SheetErrorCode {
    EmptyFile,
    UnsupportedInput,
    InvalidJson,
    InvalidJsonRoot,
    EmptySheetArray,
    InvalidSheet,
    NoNotes,
    TooManyNotes,
    InvalidNoteTime,
    InvalidNoteKey,
    KeyOutOfRange,
    DecryptFailed,
    EmptyMidi,
    InvalidMidi,
    InvalidKeyMap,
    InvalidStoredPayload,
    ParseFailed,
};

inline std::string_view sheetErrorCodeName(SheetErrorCode code)
{
    switch (code) {
    case SheetErrorCode::EmptyFile:
        return "EMPTY_FILE";
    case SheetErrorCode::UnsupportedInput:
        return "UNSUPPORTED_INPUT";
    case SheetErrorCode::InvalidJson:
        return "INVALID_JSON";
    case SheetErrorCode::InvalidJsonRoot:
        return "INVALID_JSON_ROOT";
    case SheetErrorCode::EmptySheetArray:
        return "EMPTY_SHEET_ARRAY";
    case SheetErrorCode::InvalidSheet:
        return "INVALID_SHEET";
    case SheetErrorCode::NoNotes:
        return "NO_NOTES";
    case SheetErrorCode::TooManyNotes:
        return "TOO_MANY_NOTES";
    case SheetErrorCode::InvalidNoteTime:
        return "INVALID_NOTE_TIME";
    case SheetErrorCode::InvalidNoteKey:
        return "INVALID_NOTE_KEY";
    case SheetErrorCode::KeyOutOfRange:
        return "KEY_OUT_OF_RANGE";
    case SheetErrorCode::DecryptFailed:
        return "DECRYPT_FAILED";
    case SheetErrorCode::EmptyMidi:
        return "EMPTY_MIDI";
    case SheetErrorCode::InvalidMidi:
        return "INVALID_MIDI";
    case SheetErrorCode::InvalidKeyMap:
        return "INVALID_KEYMAP";
    case SheetErrorCode::InvalidStoredPayload:
        return "INVALID_STORED_PAYLOAD";
    case SheetErrorCode::ParseFailed:
        return "PARSE_FAILED";
    }
    return "PARSE_FAILED";
}

inline Error sheetError(SheetErrorCode code, std::string message, std::string details = {})
{
    return Error{std::string(sheetErrorCodeName(code)), std::move(message), std::move(details)};
}

struct SheetWarning {
    std::string code;
    std::string message;
    int count = 0;
};

}  // namespace sky
