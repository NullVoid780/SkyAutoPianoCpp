#include "core/string_utils.hpp"

#include <algorithm>
#include <cstdint>

namespace sky {

namespace {

constexpr char32_t kReplacement = 0xFFFD;

bool isContinuation(unsigned char byte)
{
    return (byte & 0xC0) == 0x80;
}

std::size_t previousCodePointStart(std::string_view text, std::size_t end)
{
    std::size_t pos = end - 1;
    while (pos > 0 && isContinuation(static_cast<unsigned char>(text[pos]))) {
        --pos;
    }
    return pos;
}

}  // namespace

void appendUtf8(std::string &out, char32_t cp)
{
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        cp = kReplacement;
    }
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        appendUtf8(out, kReplacement);
    }
}


std::string sanitizeUtf8(std::string_view bytes)
{
    std::string out;
    out.reserve(bytes.size());
    char32_t codePoint = 0;
    int needed = 0;
    int seen = 0;
    unsigned char lower = 0x80;
    unsigned char upper = 0xBF;

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto byte = static_cast<unsigned char>(bytes[i]);
        if (needed == 0) {
            if (byte <= 0x7F) {
                out.push_back(static_cast<char>(byte));
            } else if (byte >= 0xC2 && byte <= 0xDF) {
                needed = 1;
                codePoint = byte & 0x1F;
            } else if (byte >= 0xE0 && byte <= 0xEF) {
                lower = byte == 0xE0 ? 0xA0 : 0x80;
                upper = byte == 0xED ? 0x9F : 0xBF;
                needed = 2;
                codePoint = byte & 0x0F;
            } else if (byte >= 0xF0 && byte <= 0xF4) {
                lower = byte == 0xF0 ? 0x90 : 0x80;
                upper = byte == 0xF4 ? 0x8F : 0xBF;
                needed = 3;
                codePoint = byte & 0x07;
            } else {
                appendUtf8(out, kReplacement);
            }
            continue;
        }
        if (byte < lower || byte > upper) {
            codePoint = 0;
            needed = 0;
            seen = 0;
            lower = 0x80;
            upper = 0xBF;
            appendUtf8(out, kReplacement);
            --i;
            continue;
        }
        lower = 0x80;
        upper = 0xBF;
        codePoint = (codePoint << 6) | (byte & 0x3F);
        if (++seen == needed) {
            appendUtf8(out, codePoint);
            codePoint = 0;
            needed = 0;
            seen = 0;
        }
    }
    if (needed != 0) {
        appendUtf8(out, kReplacement);
    }
    return out;
}

std::string latin1ToUtf8(std::string_view bytes)
{
    std::string out;
    out.reserve(bytes.size());
    for (char c : bytes) {
        appendUtf8(out, static_cast<unsigned char>(c));
    }
    return out;
}

char32_t decodeUtf8At(std::string_view text, std::size_t &pos)
{
    const auto lead = static_cast<unsigned char>(text[pos]);
    int extra = 0;
    char32_t cp = lead;
    if (lead >= 0xF0) {
        extra = 3;
        cp = lead & 0x07;
    } else if (lead >= 0xE0) {
        extra = 2;
        cp = lead & 0x0F;
    } else if (lead >= 0xC0) {
        extra = 1;
        cp = lead & 0x1F;
    }
    ++pos;
    for (int i = 0; i < extra && pos < text.size(); ++i, ++pos) {
        cp = (cp << 6) | (static_cast<unsigned char>(text[pos]) & 0x3F);
    }
    return cp;
}

std::string utf16ToUtf8(std::u16string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        char32_t unit = text[i];
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < text.size()) {
            const char32_t low = text[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                appendUtf8(out, 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00));
                ++i;
                continue;
            }
        }
        appendUtf8(out, unit);
    }
    return out;
}

std::u16string utf8ToUtf16(std::string_view text)
{
    const std::string valid = sanitizeUtf8(text);
    std::u16string out;
    out.reserve(valid.size());
    std::size_t pos = 0;
    while (pos < valid.size()) {
        const char32_t cp = decodeUtf8At(valid, pos);
        if (cp >= 0x10000) {
            out.push_back(static_cast<char16_t>(0xD800 + ((cp - 0x10000) >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + ((cp - 0x10000) & 0x3FF)));
        } else {
            out.push_back(static_cast<char16_t>(cp));
        }
    }
    return out;
}

std::wstring toWide(std::string_view text)
{
    const std::u16string wide = utf8ToUtf16(text);
    return std::wstring(wide.begin(), wide.end());
}

std::string fromWide(std::wstring_view text)
{
    return utf16ToUtf8(std::u16string_view(reinterpret_cast<const char16_t *>(text.data()), text.size()));
}

QString toQString(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::string fromQString(const QString &text)
{
    return text.toStdString();
}

std::size_t utf16Length(std::string_view text)
{
    std::size_t units = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        units += decodeUtf8At(text, pos) >= 0x10000 ? 2 : 1;
    }
    return units;
}

bool isJsWhitespace(char32_t cp)
{
    switch (cp) {
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x20:
    case 0xA0:
    case 0x1680:
    case 0x2028:
    case 0x2029:
    case 0x202F:
    case 0x205F:
    case 0x3000:
    case 0xFEFF:
        return true;
    default:
        return cp >= 0x2000 && cp <= 0x200A;
    }
}

std::string_view trimJs(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size()) {
        std::size_t next = begin;
        if (!isJsWhitespace(decodeUtf8At(text, next))) {
            break;
        }
        begin = next;
    }
    std::size_t end = text.size();
    while (end > begin) {
        const std::size_t start = previousCodePointStart(text, end);
        std::size_t probe = start;
        if (!isJsWhitespace(decodeUtf8At(text, probe))) {
            break;
        }
        end = start;
    }
    return text.substr(begin, end - begin);
}

std::vector<std::string_view> splitOnJsWhitespace(std::string_view text)
{
    const std::string_view trimmed = trimJs(text);
    std::vector<std::string_view> parts;
    std::size_t tokenStart = 0;
    std::size_t pos = 0;
    while (pos < trimmed.size()) {
        const std::size_t cpStart = pos;
        if (isJsWhitespace(decodeUtf8At(trimmed, pos))) {
            parts.push_back(trimmed.substr(tokenStart, cpStart - tokenStart));
            while (pos < trimmed.size()) {
                std::size_t probe = pos;
                if (!isJsWhitespace(decodeUtf8At(trimmed, probe))) {
                    break;
                }
                pos = probe;
            }
            tokenStart = pos;
        }
    }
    parts.push_back(trimmed.substr(tokenStart));
    return parts;
}

std::string toLowerAscii(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return out;
}

bool equalsIgnoreCaseAscii(std::string_view a, std::string_view b)
{
    return a.size() == b.size() && toLowerAscii(a) == toLowerAscii(b);
}

bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

}  // namespace sky
