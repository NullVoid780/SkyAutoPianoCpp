#include "codec/json_sheet_parser.hpp"

#include <string>
#include <vector>

#include "codec/relaxed_json.hpp"
#include "codec/sheet_error.hpp"

namespace sky {

namespace {

Result<OrderedJson> unwrapSheet(OrderedJson parsed)
{
    if (parsed.is_array()) {
        if (parsed.empty()) {
            return sheetError(SheetErrorCode::EmptySheetArray, "The sheet JSON array is empty.");
        }
        return std::move(parsed[0]);
    }
    if (parsed.is_object()) {
        return parsed;
    }
    return sheetError(SheetErrorCode::InvalidJsonRoot,
                      "The sheet JSON must be an object or an array containing one object.");
}

std::string parserMessage(const std::string &what)
{
    const auto close = what.find("] ");
    return close == std::string::npos ? what : what.substr(close + 2);
}

}  // namespace

Result<OrderedJson> parseJsonSheet(std::string_view text)
{
    const std::string unwrapped = stripCallWrapper(text);
    std::vector<std::string_view> candidates{text};
    if (unwrapped != text) {
        candidates.push_back(unwrapped);
    }

    for (std::string_view candidate : candidates) {
        auto parsed = parseJson<OrderedJson>(candidate);
        if (!parsed) {
            continue;
        }
        auto sheet = unwrapSheet(std::move(parsed).value());
        if (sheet) {
            return sheet;
        }
    }

    auto relaxed = parseJson<OrderedJson>(toRelaxedJson(unwrapped));
    if (!relaxed) {
        const std::string detail = parserMessage(relaxed.error().userMessage);
        return sheetError(SheetErrorCode::InvalidJson, "Invalid sheet JSON: " + detail, detail);
    }
    auto sheet = unwrapSheet(std::move(relaxed).value());
    if (!sheet) {
        const std::string &detail = sheet.error().userMessage;
        return sheetError(SheetErrorCode::InvalidJson, "Invalid sheet JSON: " + detail, detail);
    }
    return sheet;
}

}  // namespace sky
