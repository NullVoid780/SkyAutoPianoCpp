#include "services/settings/ui_state_service.hpp"

#include "core/file_system.hpp"
#include "core/logger.hpp"
#include "core/text_file.hpp"

namespace sky {

namespace {

std::vector<std::string> stringList(const OrderedJson &raw, const char *key)
{
    std::vector<std::string> values;
    const auto it = raw.find(key);
    if (it == raw.end() || !it->is_array()) {
        return values;
    }
    for (const OrderedJson &item : *it) {
        if (item.is_string()) {
            values.push_back(item.get<std::string>());
        }
    }
    return values;
}

OrderedJson toJsonArray(const std::vector<std::string> &values)
{
    OrderedJson array = OrderedJson::array();
    for (const std::string &value : values) {
        array.push_back(value);
    }
    return array;
}

}  // namespace

UiStateService::UiStateService(std::filesystem::path dataDirectory, QObject *parent)
    : QObject(parent), m_path(dataDirectory / "ui-state.json")
{
}

void UiStateService::load()
{
    m_raw = OrderedJson::object();
    std::error_code ec;
    if (std::filesystem::exists(m_path, ec)) {
        auto text = readTextFile(m_path);
        auto parsed = text ? parseJson<OrderedJson>(text.value()) : Result<OrderedJson>(text.error());
        if (parsed && parsed.value().is_object()) {
            m_raw = std::move(parsed).value();
        } else {
            Logger::warning("ui-state", "Ignoring unreadable ui-state.json");
        }
    }
    m_theme = m_raw.value("theme", OrderedJson()) == "dark" ? AppTheme::Dark : AppTheme::Light;
    m_favorites = stringList(m_raw, "favorites");
    m_recentPlays = stringList(m_raw, "recentPlays");
}

void UiStateService::save()
{
    m_raw["theme"] = m_theme == AppTheme::Dark ? "dark" : "light";
    m_raw["favorites"] = toJsonArray(m_favorites);
    m_raw["recentPlays"] = toJsonArray(m_recentPlays);
    (void)ensureDir(m_path.parent_path());
    if (auto written = atomicWrite(m_path, dumpJson(m_raw, 4)); !written) {
        Logger::error("ui-state", written.error().details);
    }
}

void UiStateService::setTheme(AppTheme theme)
{
    if (theme == m_theme) {
        return;
    }
    m_theme = theme;
    save();
    emit themeChanged(theme);
}

void UiStateService::setFavorites(std::vector<std::string> names)
{
    m_favorites = std::move(names);
    save();
    emit favoritesChanged();
}

void UiStateService::setRecentPlays(std::vector<std::string> names)
{
    m_recentPlays = std::move(names);
    save();
    emit recentPlaysChanged();
}


std::string UiStateService::themeId() const
{
    const auto found = m_raw.find("themeId");
    return found != m_raw.end() && found->is_string() ? found->get<std::string>() : std::string();
}

void UiStateService::setThemeId(const std::string &id)
{
    if (themeId() == id) {
        return;
    }
    if (id.empty()) {
        m_raw.erase("themeId");
    } else {
        m_raw["themeId"] = id;
    }
    save();
}

bool UiStateService::quickStartDone() const
{
    const auto found = m_raw.find("quickStartDone");
    return found != m_raw.end() && found->is_boolean() && found->get<bool>();
}

void UiStateService::setQuickStartDone()
{
    if (quickStartDone()) {
        return;
    }
    m_raw["quickStartDone"] = true;
    save();
}

bool UiStateService::editorAdvancedView() const
{
    const auto found = m_raw.find("editorAdvancedView");
    return found != m_raw.end() && found->is_boolean() && found->get<bool>();
}

void UiStateService::setEditorAdvancedView(bool advanced)
{
    if (editorAdvancedView() == advanced) {
        return;
    }
    m_raw["editorAdvancedView"] = advanced;
    save();
}

bool UiStateService::editorColorfulNotes() const
{
    const auto found = m_raw.find("editorColorfulNotes");
    return found == m_raw.end() || !found->is_boolean() || found->get<bool>();
}

void UiStateService::setEditorColorfulNotes(bool colorful)
{
    if (editorColorfulNotes() == colorful) {
        return;
    }
    m_raw["editorColorfulNotes"] = colorful;
    save();
}

bool UiStateService::editorSoundMuted() const
{
    const auto found = m_raw.find("editorSoundMuted");
    return found != m_raw.end() && found->is_boolean() && found->get<bool>();
}

void UiStateService::setEditorSoundMuted(bool muted)
{
    if (editorSoundMuted() == muted) {
        return;
    }
    m_raw["editorSoundMuted"] = muted;
    save();
}

bool UiStateService::followSystemTheme() const
{
    const auto found = m_raw.find("followSystemTheme");
    return found == m_raw.end() || !found->is_boolean() || found->get<bool>();
}

void UiStateService::setFollowSystemTheme(bool follow)
{
    if (followSystemTheme() == follow && m_raw.contains("followSystemTheme")) {
        return;
    }
    m_raw["followSystemTheme"] = follow;
    save();
}

bool UiStateService::legacyImportOffered() const
{
    const auto found = m_raw.find("legacyImportOffered");
    return found != m_raw.end() && found->is_boolean() && found->get<bool>();
}

void UiStateService::setLegacyImportOffered()
{
    m_raw["legacyImportOffered"] = true;
    save();
}

}  // namespace sky
