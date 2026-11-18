#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <QString>

namespace sky {

std::string utf16ToUtf8(std::u16string_view text);
std::u16string utf8ToUtf16(std::string_view text);
std::string sanitizeUtf8(std::string_view bytes);
std::string latin1ToUtf8(std::string_view bytes);

std::wstring toWide(std::string_view text);
std::string fromWide(std::wstring_view text);
QString toQString(std::string_view text);
std::string fromQString(const QString &text);

char32_t decodeUtf8At(std::string_view text, std::size_t &pos);
void appendUtf8(std::string &out, char32_t codePoint);
std::size_t utf16Length(std::string_view text);

bool isJsWhitespace(char32_t codePoint);
std::string_view trimJs(std::string_view text);
std::vector<std::string_view> splitOnJsWhitespace(std::string_view text);

std::string toLowerAscii(std::string_view text);
bool equalsIgnoreCaseAscii(std::string_view a, std::string_view b);
bool startsWith(std::string_view text, std::string_view prefix);
bool endsWith(std::string_view text, std::string_view suffix);

}  // namespace sky
