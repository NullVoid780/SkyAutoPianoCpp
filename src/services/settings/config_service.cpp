#include "services/settings/config_service.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <vector>

#include "core/file_system.hpp"
#include "core/logger.hpp"
#include "core/text_file.hpp"
#include "model/key_table.hpp"
#include "services/settings/shortcut_parser.hpp"

namespace sky {

namespace {

constexpr const char *kConfigFile = "config.json";
constexpr std::array<const char *, 5> kShortcutKeys = {"pre", "play", "next", "increaseSpeed", "decreaseSpeed"};

Error duplicateShortcut()
{
    return Error{"SHORTCUT_DUPLICATE", "Unable to save the settings, the shortcut has been duplicated!", {}};
}

bool fillMissing(OrderedJson &current, const OrderedJson &defaults, bool &updated)
{
    if (!current.is_object()) {
        return false;
    }
    for (const auto &item : defaults.items()) {
        const auto found = current.find(item.key());
        if (found == current.end()) {
            current[item.key()] = item.value();
            updated = true;
        } else if (item.value().is_object() && !fillMissing(*found, item.value(), updated)) {
            return false;
        }
    }
    return true;
}

const OrderedJson &section(const OrderedJson &json, const char *key)
{
    static const OrderedJson kEmpty = OrderedJson::object();
    if (!json.is_object()) {
        return kEmpty;
    }
    const auto it = json.find(key);
    return it != json.end() && it->is_object() ? *it : kEmpty;
}

bool truthyMember(const OrderedJson &object, const char *key)
{
    const auto it = object.find(key);
    return it != object.end() && jsTruthy(*it);
}

std::string stringMember(const OrderedJson &object, const char *key)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::string coerceToString(const OrderedJson &value)
{
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_number()) {
        return formatJsNumber(value.get<double>());
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "";
    }
    return {};
}

bool isFiniteNumber(const OrderedJson &value)
{
    return value.is_number() && std::isfinite(value.get<double>());
}

}  // namespace

ConfigService::ConfigService(std::filesystem::path configDirectory, QObject *parent)
    : QObject(parent), m_path(configDirectory / kConfigFile)
{
}

OrderedJson ConfigService::defaults()
{
    OrderedJson config = OrderedJson::object();
    config["panel"] = OrderedJson::object();
    config["panel"]["longPressMode"] = false;
    config["panel"]["speed"] = 1;
    config["panel"]["delayNext"] = 1;
    config["panel"]["autoSave"] = true;
    config["panel"]["minimizeOnPlay"] = true;
    config["panel"]["playbackMode"] = "lite";
    config["keyboard"] = OrderedJson::object();
    config["keyboard"]["customKeyboard"] = false;
    config["keyboard"]["keys"] = OrderedJson::array();
    for (std::string_view key : kAutoKeys) {
        config["keyboard"]["keys"].push_back(std::string(key));
    }
    config["shortcut"] = OrderedJson::object();
    config["shortcut"]["pre"] = "Ctrl+Shift+C";
    config["shortcut"]["play"] = "Ctrl+Shift+V";
    config["shortcut"]["next"] = "Ctrl+Shift+B";
    config["shortcut"]["increaseSpeed"] = "Ctrl+Up";
    config["shortcut"]["decreaseSpeed"] = "Ctrl+Down";
    config["update"] = OrderedJson::object();
    config["update"]["blockUpdate"] = false;
    config["appTheme"] = "dark";
    return config;
}

Config ConfigService::fromJson(const OrderedJson &json)
{
    Config config;
    const OrderedJson &panel = section(json, "panel");
    config.panel.longPressMode = truthyMember(panel, "longPressMode");
    config.panel.speed = jsMember(panel, "speed");
    config.panel.delayNext = jsMember(panel, "delayNext");
    config.panel.autoSave = truthyMember(panel, "autoSave");
    config.panel.minimizeOnPlay = truthyMember(panel, "minimizeOnPlay");
    config.panel.playbackMode = stringMember(panel, "playbackMode") == "lite" ? PlaybackMode::Lite : PlaybackMode::Strict;

    const OrderedJson &keyboard = section(json, "keyboard");
    config.keyboard.customKeyboard = truthyMember(keyboard, "customKeyboard");
    const auto keys = keyboard.find("keys");
    for (std::size_t i = 0; i < config.keyboard.keys.size(); ++i) {
        config.keyboard.keys[i] = keys != keyboard.end() && keys->is_array() && i < keys->size() && (*keys)[i].is_string()
                                      ? (*keys)[i].get<std::string>()
                                      : std::string();
    }

    const OrderedJson &shortcut = section(json, "shortcut");
    config.shortcut.pre = stringMember(shortcut, "pre");
    config.shortcut.play = stringMember(shortcut, "play");
    config.shortcut.next = stringMember(shortcut, "next");
    config.shortcut.increaseSpeed = stringMember(shortcut, "increaseSpeed");
    config.shortcut.decreaseSpeed = stringMember(shortcut, "decreaseSpeed");

    config.update.blockUpdate = truthyMember(section(json, "update"), "blockUpdate");
    config.appTheme = json.is_object() && json.value("appTheme", OrderedJson()) == "light" ? AppTheme::Light : AppTheme::Dark;
    return config;
}

OrderedJson ConfigService::loadFromDisk()
{
    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec)) {
        (void)ensureDir(m_path.parent_path());
        const OrderedJson fresh = defaults();
        if (auto written = atomicWrite(m_path, dumpJson(fresh, 4)); !written) {
            Logger::error("config", written.error().details);
        }
        return fresh;
    }

    auto text = readTextFile(m_path);
    auto parsed = text ? parseJson<OrderedJson>(text.value()) : Result<OrderedJson>(text.error());
    bool updated = false;
    if (parsed && fillMissing(parsed.value(), defaults(), updated)) {
        if (updated) {
            if (auto written = atomicWrite(m_path, dumpJson(parsed.value(), 4)); !written) {
                Logger::error("config", written.error().details);
            }
        }
        return std::move(parsed).value();
    }

    Logger::warning("config", "Failed to load config, restoring defaults");
    const OrderedJson fresh = defaults();
    if (auto written = atomicWrite(m_path, dumpJson(fresh, 4)); !written) {
        Logger::error("config", written.error().details);
    }
    return fresh;
}

void ConfigService::load()
{
    OrderedJson loaded = loadFromDisk();
    {
        std::lock_guard lock(m_mutex);
        m_raw = std::move(loaded);
        m_config = fromJson(m_raw);
        m_speed.store(safeSpeed(m_config.panel), std::memory_order_relaxed);
    }
    emit changed();
}

Result<void> ConfigService::reloadFromDisk()
{
    load();
    return {};
}

Config ConfigService::value() const
{
    std::lock_guard lock(m_mutex);
    return m_config;
}

double ConfigService::speed() const
{
    return m_speed.load(std::memory_order_relaxed);
}

PanelConfig ConfigService::panel() const
{
    std::lock_guard lock(m_mutex);
    return m_config.panel;
}

KeyboardConfig ConfigService::keyboard() const
{
    std::lock_guard lock(m_mutex);
    return m_config.keyboard;
}

ShortcutConfig ConfigService::shortcuts() const
{
    std::lock_guard lock(m_mutex);
    return m_config.shortcut;
}

OrderedJson ConfigService::raw() const
{
    std::lock_guard lock(m_mutex);
    return m_raw;
}

Result<void> ConfigService::persistLocked()
{
    m_config = fromJson(m_raw);
    m_speed.store(safeSpeed(m_config.panel), std::memory_order_relaxed);
    auto written = atomicWrite(m_path, dumpJson(m_raw, 4));
    if (!written) {
        Logger::error("config", written.error().details);
        return Error{"CONFIG_WRITE_FAILED", "Can't save the settings!", written.error().details};
    }
    return {};
}

void ConfigService::setTheme(AppTheme theme)
{
    {
        std::lock_guard lock(m_mutex);
        const std::string value = theme == AppTheme::Light ? "light" : "dark";
        if (m_raw.value("appTheme", OrderedJson()) == value) {
            return;
        }
        m_raw["appTheme"] = value;
        (void)persistLocked();
    }
    emit changed();
}

void ConfigService::mergeSection(const char *name, const OrderedJson &partial)
{
    {
        std::lock_guard lock(m_mutex);
        OrderedJson &target = m_raw[name];
        if (!target.is_object()) {
            target = OrderedJson::object();
        }
        if (partial.is_object()) {
            for (const auto &item : partial.items()) {
                target[item.key()] = item.value();
            }
        }
        (void)persistLocked();
    }
    emit changed();
}

void ConfigService::updatePanel(const OrderedJson &partial)
{
    mergeSection("panel", partial);
}

void ConfigService::updateShortcut(const OrderedJson &partial)
{
    mergeSection("shortcut", partial);
}

void ConfigService::updateKeyboard(const OrderedJson &partial)
{
    mergeSection("keyboard", partial);
}

Result<void> ConfigService::replace(const OrderedJson &config)
{
    if (!config.is_object()) {
        return Error{"CONFIG_INVALID", "Can't save the settings!", {}};
    }
    Result<void> result;
    {
        std::lock_guard lock(m_mutex);
        m_raw = config;
        result = persistLocked();
    }
    emit changed();
    return result;
}

Result<void> ConfigService::validateShortcuts(const ShortcutConfig &shortcuts)
{
    const std::array<std::string_view, 5> values = {shortcuts.pre, shortcuts.play, shortcuts.next,
                                                    shortcuts.increaseSpeed, shortcuts.decreaseSpeed};
    std::set<std::string> seen;
    for (std::string_view raw : values) {
        const std::string value(trimJs(raw));
        if (value.empty()) {
            return Error{"SHORTCUT_EMPTY", "Unable to save the settings, a shortcut is empty!", {}};
        }
        if (!seen.insert(value).second) {
            return duplicateShortcut();
        }
    }
    std::vector<ParsedShortcut> parsed;
    for (std::string_view raw : values) {
        auto shortcut = parseShortcut(trimJs(raw));
        if (!shortcut) {
            return shortcut.error();
        }
        const auto same = [&](const ParsedShortcut &other) {
            return other.key == shortcut.value().key && other.modifiers == shortcut.value().modifiers;
        };
        if (std::any_of(parsed.begin(), parsed.end(), same)) {
            return duplicateShortcut();
        }
        parsed.push_back(shortcut.value());
    }
    return {};
}

Result<void> ConfigService::writeSettingsFile(const std::filesystem::path &file, const OrderedJson &config)
{
    if (auto written = atomicWrite(file, dumpJson(config, 4)); !written) {
        return Error{"SETTINGS_EXPORT_FAILED", "Can't export the settings!", written.error().details};
    }
    return {};
}

Result<OrderedJson> ConfigService::readSettingsFile(const std::filesystem::path &file)
{
    auto text = readTextFile(file);
    if (!text) {
        return Error{"SETTINGS_INVALID", "The selected settings file is invalid.", text.error().details};
    }
    auto parsed = parseJson<OrderedJson>(text.value());
    if (!parsed) {
        return Error{"SETTINGS_INVALID", "The selected settings file is invalid.", parsed.error().userMessage};
    }
    return parsed.value();
}

Result<OrderedJson> ConfigService::normalizeImported(const OrderedJson &current, const OrderedJson &imported)
{
    if (!imported.is_object()) {
        return Error{"SETTINGS_INVALID", "The selected settings file is invalid.", {}};
    }
    OrderedJson next = current;

    const auto panelIt = imported.find("panel");
    if (panelIt != imported.end() && panelIt->is_object()) {
        const OrderedJson &panel = *panelIt;
        OrderedJson &target = next["panel"];
        if (panel.contains("longPressMode") && panel["longPressMode"].is_boolean()) {
            target["longPressMode"] = panel["longPressMode"];
        }
        if (panel.contains("speed") && isFiniteNumber(panel["speed"]) && panel["speed"].get<double>() >= 0.1 &&
            panel["speed"].get<double>() <= 5) {
            target["speed"] = panel["speed"];
        }
        if (panel.contains("delayNext") && isFiniteNumber(panel["delayNext"]) && panel["delayNext"].get<double>() >= 0) {
            target["delayNext"] = panel["delayNext"];
        }
        if (panel.contains("autoSave") && panel["autoSave"].is_boolean()) {
            target["autoSave"] = panel["autoSave"];
        }
        if (panel.contains("minimizeOnPlay") && panel["minimizeOnPlay"].is_boolean()) {
            target["minimizeOnPlay"] = panel["minimizeOnPlay"];
        }
        if (panel.contains("playbackMode") && (panel["playbackMode"] == "lite" || panel["playbackMode"] == "strict")) {
            target["playbackMode"] = panel["playbackMode"];
        }
    }

    const auto keyboardIt = imported.find("keyboard");
    if (keyboardIt != imported.end() && keyboardIt->is_object()) {
        const OrderedJson &keyboard = *keyboardIt;
        OrderedJson &target = next["keyboard"];
        if (keyboard.contains("customKeyboard") && keyboard["customKeyboard"].is_boolean()) {
            target["customKeyboard"] = keyboard["customKeyboard"];
        }
        if (keyboard.contains("keys") && keyboard["keys"].is_array() && target.contains("keys") &&
            target["keys"].is_array()) {
            const OrderedJson &incoming = keyboard["keys"];
            OrderedJson &keys = target["keys"];
            for (std::size_t i = 0; i < keys.size(); ++i) {
                if (i >= incoming.size()) {
                    continue;
                }
                const std::string normalized = normalizeKeyboardMappingValue(coerceToString(incoming[i]));
                if (!normalized.empty()) {
                    keys[i] = normalized;
                }
            }
        }
    }

    const auto shortcutIt = imported.find("shortcut");
    if (shortcutIt != imported.end() && shortcutIt->is_object()) {
        for (const char *key : kShortcutKeys) {
            const auto value = shortcutIt->find(key);
            if (value != shortcutIt->end() && value->is_string()) {
                const std::string trimmed(trimJs(value->get_ref<const std::string &>()));
                if (!trimmed.empty()) {
                    next["shortcut"][key] = trimmed;
                }
            }
        }
    }

    const auto updateIt = imported.find("update");
    if (updateIt != imported.end() && updateIt->is_object()) {
        const auto block = updateIt->find("blockUpdate");
        if (block != updateIt->end() && block->is_boolean()) {
            next["update"]["blockUpdate"] = *block;
        }
    }

    const auto theme = imported.find("appTheme");
    if (theme != imported.end() && (*theme == "light" || *theme == "dark")) {
        next["appTheme"] = *theme;
    }
    return next;
}

double ConfigService::safeSpeed(const PanelConfig &panel)
{
    return std::isfinite(panel.speed) && panel.speed > 0 ? panel.speed : 1.0;
}

double ConfigService::safeDelayNextMs(const PanelConfig &panel)
{
    if (!std::isfinite(panel.delayNext) || panel.delayNext < 0) {
        return 0.0;
    }
    return panel.delayNext * 1000.0;
}

}  // namespace sky
