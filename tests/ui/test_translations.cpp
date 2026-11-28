#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/translator.hpp"

namespace {

const std::filesystem::path kRoot = std::filesystem::path(SKY_SOURCE_DIR);
const std::vector<std::string> kLanguages = {"vi", "zh_CN", "ja", "es"};

std::string readText(const std::filesystem::path &file)
{
    std::ifstream input(file, std::ios::binary);
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

std::vector<std::filesystem::path> sourceFiles()
{
    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(kRoot / "src")) {
        const std::string extension = entry.path().extension().string();
        if (entry.is_regular_file() && (extension == ".cpp" || extension == ".hpp")) {
            files.push_back(entry.path());
        }
    }
    return files;
}

bool identifierChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::size_t skipSpace(const std::string &text, std::size_t pos)
{
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
    return pos;
}

bool readLiteral(const std::string &text, std::size_t &pos, std::string &out)
{
    if (pos >= text.size() || text[pos] != '"') {
        return false;
    }
    ++pos;
    while (pos < text.size() && text[pos] != '"') {
        if (text[pos] == '\\' && pos + 1 < text.size()) {
            const char next = text[pos + 1];
            out += next == 'n' ? '\n' : next == 't' ? '\t' : next;
            pos += 2;
        } else {
            out += text[pos++];
        }
    }
    ++pos;
    return true;
}

std::map<std::string, std::string> translatableKeys()
{
    std::map<std::string, std::string> keys;
    for (const auto &file : sourceFiles()) {
        const std::string text = readText(file);
        for (const std::string call : {"tr(", "trUtf8(", "trNoop("}) {
            std::size_t found = 0;
            while ((found = text.find(call, found)) != std::string::npos) {
                const std::size_t start = found;
                found += call.size();
                if (start > 0 && identifierChar(text[start - 1])) {
                    continue;
                }
                std::size_t pos = skipSpace(text, found);
                std::string key;
                bool any = false;
                while (readLiteral(text, pos, key)) {
                    any = true;
                    pos = skipSpace(text, pos);
                }
                if (any && pos < text.size() && (text[pos] == ',' || text[pos] == ')')) {
                    keys.emplace(key, std::filesystem::relative(file, kRoot).generic_string());
                }
            }
        }
    }
    return keys;
}

QJsonObject languageTable(const std::string &language)
{
    QFile file(QStringLiteral(":/i18n/%1.json").arg(QString::fromStdString(language)));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    REQUIRE(document.isObject());
    return document.object();
}

QStringList placeholders(const QString &text)
{
    QStringList found;
    static const QRegularExpression pattern(QStringLiteral("%\\d"));
    for (auto it = pattern.globalMatch(text); it.hasNext();) {
        found << it.next().captured();
    }
    found.sort();
    return found;
}

std::string joined(const std::vector<std::string> &items)
{
    std::string text;
    for (const std::string &item : items) {
        text += "\n  " + item;
    }
    return text;
}

}  // namespace

TEST_CASE("every translatable text has a translation in each language", "[i18n]")
{
    const auto keys = translatableKeys();
    REQUIRE(keys.size() > 500);
    for (const std::string &language : kLanguages) {
        const QJsonObject table = languageTable(language);
        std::vector<std::string> missing;
        for (const auto &[key, file] : keys) {
            const QJsonValue value = table.value(QString::fromStdString(key));
            if (!value.isString() || value.toString().trimmed().isEmpty()) {
                missing.push_back(file + ": " + key);
            }
        }
        INFO(language << " is missing:" << joined(missing));
        CHECK(missing.empty());
    }
}

TEST_CASE("language files only hold text the app uses", "[i18n]")
{
    const auto keys = translatableKeys();
    for (const std::string &language : kLanguages) {
        std::vector<std::string> unused;
        const QJsonObject table = languageTable(language);
        for (auto it = table.begin(); it != table.end(); ++it) {
            if (!keys.contains(it.key().toStdString())) {
                unused.push_back(it.key().toStdString());
            }
        }
        INFO(language << " has unused entries:" << joined(unused));
        CHECK(unused.empty());
    }
}

TEST_CASE("translations keep the placeholders, line breaks and markup", "[i18n]")
{
    static const QRegularExpression tag(QStringLiteral("<[^>]+>"));
    for (const std::string &language : kLanguages) {
        std::vector<std::string> broken;
        const QJsonObject table = languageTable(language);
        for (auto it = table.begin(); it != table.end(); ++it) {
            const QString source = it.key();
            const QString value = it.value().toString();
            const bool sameMarkup = source.count(tag) == value.count(tag) &&
                                    source.count(QLatin1Char('\n')) == value.count(QLatin1Char('\n')) &&
                                    source.contains(QStringLiteral("[*]")) == value.contains(QStringLiteral("[*]"));
            if (placeholders(source) != placeholders(value) || !sameMarkup) {
                broken.push_back(source.toStdString() + " -> " + value.toStdString());
            }
        }
        INFO(language << " has broken entries:" << joined(broken));
        CHECK(broken.empty());
    }
}

TEST_CASE("visible text in the interface goes through the translator", "[i18n]")
{
    const std::vector<std::regex> patterns = {
        std::regex(R"re(\b(?:setText|setToolTip|setPlaceholderText|setWindowTitle|setAccessibleName|addOption|setLabel|new QLabel|new QPushButton|new Badge)\(\s*QStringLiteral\("([^"]*)")re"),
        std::regex(R"re(\bdrawText\([^;]*?QStringLiteral\("([^"]*)")re"),
    };
    const std::set<std::string> allowed = {"Sky Auto Piano", "Alt+Tab"};
    std::vector<std::string> untranslated;
    for (const auto &file : sourceFiles()) {
        const std::string relative = std::filesystem::relative(file, kRoot).generic_string();
        if (relative.rfind("src/ui/", 0) != 0 && relative.rfind("src/app/", 0) != 0) {
            continue;
        }
        const std::string text = readText(file);
        for (const std::regex &pattern : patterns) {
            for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern); it != std::sregex_iterator(); ++it) {
                const std::string literal = (*it)[1].str();
                const bool hasLetters = std::any_of(literal.begin(), literal.end(),
                                                    [](unsigned char c) { return std::isalpha(c) != 0; });
                if (hasLetters && !allowed.contains(literal) && literal.front() != '<') {
                    untranslated.push_back(relative + ": " + literal);
                }
            }
        }
    }
    INFO("wrap these in tr():" << joined(untranslated));
    CHECK(untranslated.empty());
}

TEST_CASE("translator loads every language and falls back to English", "[i18n]")
{
    for (const std::string &language : kLanguages) {
        sky::Translator translator;
        REQUIRE(translator.loadLanguage(QString::fromStdString(language)));
        CHECK(translator.size() > 500);
        CHECK_FALSE(translator.lookup(QStringLiteral("Settings")).isEmpty());
        CHECK(translator.translate("sky", "a text the app never shows").isEmpty());
    }
    sky::Translator english;
    REQUIRE(english.loadLanguage(QStringLiteral("en")));
    CHECK(english.isEmpty());
}

TEST_CASE("language preference resolves against the Windows languages", "[i18n]")
{
    CHECK(sky::resolveLanguage(QStringLiteral("ja"), {QStringLiteral("vi-VN")}) == QStringLiteral("ja"));
    CHECK(sky::resolveLanguage(QString(), {QStringLiteral("vi-VN")}) == QStringLiteral("vi"));
    CHECK(sky::resolveLanguage(QString(), {QStringLiteral("zh-Hans-CN")}) == QStringLiteral("zh_CN"));
    CHECK(sky::resolveLanguage(QString(), {QStringLiteral("zh-TW")}) == QStringLiteral("zh_CN"));
    CHECK(sky::resolveLanguage(QString(), {QStringLiteral("ja-JP")}) == QStringLiteral("ja"));
    CHECK(sky::resolveLanguage(QString(), {QStringLiteral("es-419")}) == QStringLiteral("es"));
    CHECK(sky::resolveLanguage(QString(), {QStringLiteral("fr-FR"), QStringLiteral("es-ES")}) == QStringLiteral("es"));
    CHECK(sky::resolveLanguage(QString(), {QStringLiteral("fr-FR")}) == QStringLiteral("en"));
    CHECK(sky::resolveLanguage(QStringLiteral("xx"), {QStringLiteral("en-US")}) == QStringLiteral("en"));
}
