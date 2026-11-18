#include "codec/relaxed_json.hpp"

#include <cstdio>

#include "core/string_utils.hpp"

namespace sky {

namespace {

std::size_t skipWhitespaceForward(std::string_view text, std::size_t pos)
{
    while (pos < text.size()) {
        std::size_t next = pos;
        if (!isJsWhitespace(decodeUtf8At(text, next))) {
            break;
        }
        pos = next;
    }
    return pos;
}

std::size_t skipWhitespaceBackward(std::string_view text, std::size_t end)
{
    while (end > 0) {
        std::size_t start = end - 1;
        while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
            --start;
        }
        std::size_t probe = start;
        if (!isJsWhitespace(decodeUtf8At(text, probe))) {
            break;
        }
        end = start;
    }
    return end;
}

bool isIdentifierStart(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '$';
}

bool isIdentifierPart(char c)
{
    return isIdentifierStart(c) || (c >= '0' && c <= '9');
}

bool startsLineTerminator(std::string_view text, std::size_t pos)
{
    std::size_t probe = pos;
    const char32_t cp = decodeUtf8At(text, probe);
    return cp == U'\n' || cp == U'\r' || cp == 0x2028 || cp == 0x2029;
}

std::string replaceAll(std::string text, std::string_view from, std::string_view to)
{
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::string quoteBareKeys(std::string_view text)
{
    std::string out;
    out.reserve(text.size() + 16);
    std::size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '{' || c == ',') {
            const std::size_t identStart = skipWhitespaceForward(text, i + 1);
            if (identStart < text.size() && isIdentifierStart(text[identStart])) {
                std::size_t identEnd = identStart + 1;
                while (identEnd < text.size() && isIdentifierPart(text[identEnd])) {
                    ++identEnd;
                }
                const std::size_t colon = skipWhitespaceForward(text, identEnd);
                if (colon < text.size() && text[colon] == ':') {
                    out.append(text.substr(i, identStart - i));
                    out.push_back('"');
                    out.append(text.substr(identStart, identEnd - identStart));
                    out.push_back('"');
                    out.append(text.substr(identEnd, colon + 1 - identEnd));
                    i = colon + 1;
                    continue;
                }
            }
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

std::string convertSingleQuotedStrings(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\'') {
            std::size_t j = i + 1;
            bool closed = false;
            while (j < text.size()) {
                if (text[j] == '\\') {
                    if (j + 1 >= text.size() || startsLineTerminator(text, j + 1)) {
                        break;
                    }
                    std::size_t next = j + 1;
                    decodeUtf8At(text, next);
                    j = next;
                    continue;
                }
                if (text[j] == '\'') {
                    closed = true;
                    break;
                }
                ++j;
            }
            if (closed) {
                std::string inner(text.substr(i + 1, j - i - 1));
                inner = replaceAll(std::move(inner), "\\'", "'");
                inner = replaceAll(std::move(inner), "\\\"", "\"");
                out.append(jsonStringLiteral(inner));
                i = j + 1;
                continue;
            }
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

std::string removeTrailingCommas(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == ',') {
            const std::size_t close = skipWhitespaceForward(text, i + 1);
            if (close < text.size() && (text[close] == '}' || text[close] == ']')) {
                out.push_back(text[close]);
                i = close + 1;
                continue;
            }
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

}  // namespace

std::string stripCallWrapper(std::string_view text)
{
    std::string_view rest = text;
    if (!rest.empty() && rest.front() == '(') {
        rest = rest.substr(skipWhitespaceForward(rest, 1));
    }
    const std::size_t contentEnd = skipWhitespaceBackward(rest, rest.size());
    std::size_t close = std::string_view::npos;
    if (contentEnd >= 1 && rest[contentEnd - 1] == ')') {
        close = contentEnd - 1;
    } else if (contentEnd >= 2 && rest[contentEnd - 1] == ';' && rest[contentEnd - 2] == ')') {
        close = contentEnd - 2;
    }
    if (close == std::string_view::npos) {
        return std::string(rest);
    }
    return std::string(rest.substr(0, skipWhitespaceBackward(rest, close)));
}

std::string toRelaxedJson(std::string_view text)
{
    return removeTrailingCommas(convertSingleQuotedStrings(quoteBareKeys(text)));
}

std::string jsonStringLiteral(std::string_view text)
{
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (char c : text) {
        switch (c) {
        case '"':
            out.append("\\\"");
            break;
        case '\\':
            out.append("\\\\");
            break;
        case '\b':
            out.append("\\b");
            break;
        case '\f':
            out.append("\\f");
            break;
        case '\n':
            out.append("\\n");
            break;
        case '\r':
            out.append("\\r");
            break;
        case '\t':
            out.append("\\t");
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                out.append(buffer);
            } else {
                out.push_back(c);
            }
        }
    }
    out.push_back('"');
    return out;
}

}  // namespace sky
