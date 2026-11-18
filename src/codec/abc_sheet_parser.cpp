#include "codec/abc_sheet_parser.hpp"

#include <cmath>
#include <string>
#include <vector>

#include "core/string_utils.hpp"
#include "model/key_table.hpp"

namespace sky {

namespace {

constexpr std::string_view kMarker = "<DontCopyThisLine>";

bool isNoteLetter(char c)
{
    return c >= 'A' && c <= 'C';
}

bool isNoteDigit(char c)
{
    return c >= '1' && c <= '5';
}

bool whitespaceBefore(std::string_view text, std::size_t pos)
{
    if (pos == 0) {
        return true;
    }
    std::size_t start = pos - 1;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
        --start;
    }
    return isJsWhitespace(decodeUtf8At(text, start));
}

bool whitespaceAt(std::string_view text, std::size_t pos)
{
    if (pos >= text.size()) {
        return true;
    }
    return isJsWhitespace(decodeUtf8At(text, pos));
}

double jsParseInt(const std::vector<std::string_view> &tokens, std::size_t index)
{
    if (index >= tokens.size()) {
        return kNaN;
    }
    std::string_view text = trimJs(tokens[index]);
    bool negative = false;
    if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    double value = 0.0;
    bool digits = false;
    for (char c : text) {
        if (c < '0' || c > '9') {
            break;
        }
        value = value * 10.0 + (c - '0');
        digits = true;
    }
    if (!digits) {
        return kNaN;
    }
    return negative ? -value : value;
}

double orDefault(double value, double fallback)
{
    return (std::isnan(value) || value == 0.0) ? fallback : value;
}

std::vector<std::string_view> splitLines(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (true) {
        const std::size_t newline = text.find('\n', start);
        if (newline == std::string_view::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        std::size_t end = newline;
        if (end > start && text[end - 1] == '\r') {
            --end;
        }
        lines.push_back(text.substr(start, end - start));
        start = newline + 1;
    }
    return lines;
}

}  // namespace

bool looksLikeAbc(std::string_view text)
{
    if (startsWith(text, kMarker)) {
        return true;
    }
    for (std::size_t i = 0; i + 1 < text.size(); ++i) {
        if (isNoteLetter(text[i]) && isNoteDigit(text[i + 1]) && whitespaceBefore(text, i) &&
            whitespaceAt(text, i + 2)) {
            return true;
        }
    }
    return false;
}

OrderedJson parseAbcSheet(std::string_view text, std::string_view name)
{
    const std::vector<std::string_view> lines = splitLines(text);
    const std::vector<std::string_view> meta = splitOnJsWhitespace(lines[0]);
    const std::size_t offset = meta[0] == kMarker ? 1 : 0;

    const double bpm = orDefault(jsParseInt(meta, offset), 240.0);
    const double pitchLevel = orDefault(jsParseInt(meta, offset + 1), 0.0);
    const double bitsPerPage = orDefault(jsParseInt(meta, offset + 2), 16.0);
    const auto textOr = [&meta](std::size_t index) {
        return index < meta.size() && !meta[index].empty() ? std::string(meta[index]) : std::string("Unknown");
    };
    const std::string author = textOr(offset + 3);
    const std::string transcribedBy = textOr(offset + 4);
    const double step = std::floor(60000.0 / bpm);

    OrderedJson songNotes = OrderedJson::array();
    double time = 0.0;
    for (std::size_t lineIndex = 1; lineIndex < lines.size(); ++lineIndex) {
        for (std::string_view token : splitOnJsWhitespace(lines[lineIndex])) {
            if (!token.empty() && token != ".") {
                std::size_t i = 0;
                while (i + 1 < token.size()) {
                    if (isNoteLetter(token[i]) && isNoteDigit(token[i + 1])) {
                        const int keyIndex = (token[i] - 'A') * 5 + (token[i + 1] - '1');
                        OrderedJson note = OrderedJson::object();
                        note["time"] = jsonNumber(time);
                        note["key"] = songKeyName(keyIndex);
                        songNotes.push_back(std::move(note));
                        i += 2;
                    } else {
                        ++i;
                    }
                }
            }
            time += step;
        }
    }

    OrderedJson sheet = OrderedJson::object();
    sheet["name"] = std::string(name);
    sheet["author"] = author;
    sheet["transcribedBy"] = transcribedBy;
    sheet["bpm"] = jsonNumber(bpm);
    sheet["bitsPerPage"] = jsonNumber(bitsPerPage);
    sheet["pitchLevel"] = jsonNumber(pitchLevel);
    sheet["isEncrypted"] = false;
    sheet["songNotes"] = std::move(songNotes);
    return sheet;
}

}  // namespace sky
