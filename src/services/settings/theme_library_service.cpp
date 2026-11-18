#include "services/settings/theme_library_service.hpp"

#include <algorithm>
#include <cctype>

#include <QString>

#include "core/file_system.hpp"
#include "core/logger.hpp"
#include "core/random.hpp"
#include "core/string_utils.hpp"
#include "core/text_file.hpp"
#include "core/time_utils.hpp"

namespace sky {

namespace {

constexpr const char *kFormat = "sky-auto-piano-theme";
constexpr int kFormatVersion = 2;
constexpr std::size_t kSlotIdLimit = 64;

Error invalidFile()
{
    return Error{"THEME_INVALID", "The selected theme file is invalid.", {}};
}

bool isSlotId(const std::string &id)
{
    if (id.empty() || id.size() > kSlotIdLimit) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isalnum(c) || c == '.' || c == '_'; });
}

ThemeColorList readColors(const OrderedJson &colors)
{
    ThemeColorList result;
    for (const auto &[key, value] : colors.items()) {
        if (result.size() >= ThemeLibraryService::kColorLimit) {
            break;
        }
        if (!isSlotId(key) || !value.is_string()) {
            continue;
        }
        std::string text = value.get<std::string>();
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ThemeLibraryService::isColorText(text)) {
            result.emplace_back(key, std::move(text));
        }
    }
    return result;
}

OrderedJson writeColors(const ThemeColorList &colors)
{
    OrderedJson json = OrderedJson::object();
    for (const auto &[key, value] : colors) {
        json[key] = value;
    }
    return json;
}

void cleanColors(ThemeColorList &colors)
{
    colors.erase(std::remove_if(colors.begin(), colors.end(),
                                [](const auto &entry) {
                                    return !isSlotId(entry.first) || !ThemeLibraryService::isColorText(entry.second);
                                }),
                 colors.end());
    if (colors.size() > ThemeLibraryService::kColorLimit) {
        colors.resize(ThemeLibraryService::kColorLimit);
    }
}

std::string lowerName(const std::string &name)
{
    return fromQString(toQString(name).toCaseFolded());
}

}  // namespace

ThemeLibraryService::ThemeLibraryService(std::filesystem::path dataDirectory, QObject *parent)
    : QObject(parent), m_path(dataDirectory / "themes.json")
{
}

std::string ThemeLibraryService::cleanName(const std::string &name)
{
    QString text = toQString(name).simplified();
    if (text.size() > kNameLimit) {
        text = text.left(kNameLimit).trimmed();
    }
    return fromQString(text);
}

bool ThemeLibraryService::isColorText(const std::string &text)
{
    if (text.size() != 7 && text.size() != 9) {
        return false;
    }
    if (text[0] != '#') {
        return false;
    }
    return std::all_of(text.begin() + 1, text.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

Result<CustomTheme> ThemeLibraryService::fromJson(const OrderedJson &json)
{
    if (!json.is_object()) {
        return invalidFile();
    }
    CustomTheme theme;
    if (const auto id = json.find("id"); id != json.end() && id->is_string()) {
        theme.id = id->get<std::string>();
    }
    const auto name = json.find("name");
    if (name == json.end() || !name->is_string()) {
        return invalidFile();
    }
    theme.name = cleanName(name->get<std::string>());
    if (theme.name.empty()) {
        return invalidFile();
    }
    const auto light = json.find("light");
    const auto dark = json.find("dark");
    if (light != json.end() || dark != json.end()) {
        if ((light != json.end() && !light->is_object()) || (dark != json.end() && !dark->is_object())) {
            return invalidFile();
        }
        if (light != json.end()) {
            theme.light = readColors(*light);
        }
        if (dark != json.end()) {
            theme.dark = readColors(*dark);
        }
        return theme;
    }
    const auto base = json.find("base");
    const auto colors = json.find("colors");
    if (base == json.end() || !base->is_string() || (*base != "light" && *base != "dark") || colors == json.end() ||
        !colors->is_object()) {
        return invalidFile();
    }
    (*base == "dark" ? theme.dark : theme.light) = readColors(*colors);
    return theme;
}

OrderedJson ThemeLibraryService::toJson(const CustomTheme &theme, bool withId)
{
    OrderedJson json = OrderedJson::object();
    if (withId) {
        json["id"] = theme.id;
    } else {
        json["format"] = kFormat;
        json["version"] = kFormatVersion;
    }
    json["name"] = theme.name;
    json["light"] = writeColors(theme.light);
    json["dark"] = writeColors(theme.dark);
    return json;
}

void ThemeLibraryService::load()
{
    m_themes.clear();
    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec)) {
        return;
    }
    auto text = readTextFile(m_path);
    auto parsed = text ? parseJson<OrderedJson>(text.value()) : Result<OrderedJson>(text.error());
    if (!parsed || !parsed.value().is_object()) {
        Logger::warning("themes", "Ignoring unreadable themes.json");
        return;
    }
    const auto list = parsed.value().find("themes");
    if (list == parsed.value().end() || !list->is_array()) {
        return;
    }
    for (const OrderedJson &item : *list) {
        auto theme = fromJson(item);
        if (!theme || theme.value().id.empty() || find(theme.value().id) || m_themes.size() >= kThemeLimit) {
            continue;
        }
        m_themes.push_back(std::move(theme).value());
    }
}

Result<void> ThemeLibraryService::write() const
{
    OrderedJson list = OrderedJson::array();
    for (const CustomTheme &theme : m_themes) {
        list.push_back(toJson(theme, true));
    }
    OrderedJson root = OrderedJson::object();
    root["themes"] = std::move(list);
    (void)ensureDir(m_path.parent_path());
    if (auto written = atomicWrite(m_path, dumpJson(root, 4)); !written) {
        Logger::error("themes", written.error().details);
        return Error{"THEME_SAVE_FAILED", "The theme could not be saved.", written.error().details};
    }
    return {};
}

const CustomTheme *ThemeLibraryService::find(const std::string &id) const
{
    const auto found = std::find_if(m_themes.begin(), m_themes.end(), [&id](const CustomTheme &t) { return t.id == id; });
    return found == m_themes.end() ? nullptr : &*found;
}

std::string ThemeLibraryService::uniqueName(const std::string &wanted, const std::string &ignoreId) const
{
    const std::string base = cleanName(wanted);
    const auto taken = [this, &ignoreId](const std::string &name) {
        const std::string folded = lowerName(name);
        return std::any_of(m_themes.begin(), m_themes.end(), [&](const CustomTheme &t) {
            return t.id != ignoreId && lowerName(t.name) == folded;
        });
    };
    if (!taken(base)) {
        return base;
    }
    for (int n = 2;; ++n) {
        const std::string suffix = " " + std::to_string(n);
        QString trimmed = toQString(base);
        const qsizetype room = kNameLimit - static_cast<qsizetype>(suffix.size());
        if (trimmed.size() > room) {
            trimmed = trimmed.left(room).trimmed();
        }
        const std::string candidate = fromQString(trimmed) + suffix;
        if (!taken(candidate)) {
            return candidate;
        }
    }
}

Result<std::string> ThemeLibraryService::save(CustomTheme theme)
{
    theme.name = cleanName(theme.name);
    if (theme.name.empty()) {
        return Error{"THEME_NAME_EMPTY", "Enter a theme name.", {}};
    }
    cleanColors(theme.light);
    cleanColors(theme.dark);
    const auto existing = std::find_if(m_themes.begin(), m_themes.end(), [&theme](const CustomTheme &t) {
        return !theme.id.empty() && t.id == theme.id;
    });
    theme.name = uniqueName(theme.name, existing == m_themes.end() ? std::string() : theme.id);
    std::vector<CustomTheme> previous = m_themes;
    if (existing != m_themes.end()) {
        *existing = theme;
    } else {
        if (m_themes.size() >= kThemeLimit) {
            return Error{"THEME_LIMIT", "You can keep up to 100 custom themes.", {}};
        }
        theme.id = "theme-" + std::to_string(epochMillis()) + "-" + randomBase36(6);
        m_themes.push_back(theme);
    }
    if (auto written = write(); !written) {
        m_themes = std::move(previous);
        return written.error();
    }
    emit changed();
    return theme.id;
}

bool ThemeLibraryService::remove(const std::string &id)
{
    const auto found = std::find_if(m_themes.begin(), m_themes.end(), [&id](const CustomTheme &t) { return t.id == id; });
    if (found == m_themes.end()) {
        return false;
    }
    std::vector<CustomTheme> previous = m_themes;
    m_themes.erase(found);
    if (!write()) {
        m_themes = std::move(previous);
        return false;
    }
    emit changed();
    return true;
}

Result<CustomTheme> ThemeLibraryService::readThemeFile(const std::filesystem::path &file)
{
    auto text = readTextFile(file);
    if (!text) {
        return invalidFile();
    }
    auto parsed = parseJson<OrderedJson>(text.value());
    if (!parsed || !parsed.value().is_object()) {
        return invalidFile();
    }
    const auto format = parsed.value().find("format");
    if (format == parsed.value().end() || *format != kFormat) {
        return invalidFile();
    }
    auto theme = fromJson(parsed.value());
    if (!theme) {
        return theme.error();
    }
    theme.value().id.clear();
    return theme;
}

Result<void> ThemeLibraryService::writeThemeFile(const std::filesystem::path &file, const CustomTheme &theme)
{
    if (auto written = atomicWrite(file, dumpJson(toJson(theme, false), 4)); !written) {
        return Error{"THEME_EXPORT_FAILED", "The theme could not be exported.", written.error().details};
    }
    return {};
}

}  // namespace sky
