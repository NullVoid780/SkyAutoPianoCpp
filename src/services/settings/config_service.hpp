#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>

#include <QObject>

#include "core/json.hpp"
#include "core/result.hpp"
#include "model/config.hpp"

namespace sky {

class ConfigService final : public QObject {
    Q_OBJECT

public:
    explicit ConfigService(std::filesystem::path configDirectory, QObject *parent = nullptr);

    void load();
    Result<void> reloadFromDisk();

    Config value() const;
    PanelConfig panel() const;
    double speed() const;
    KeyboardConfig keyboard() const;
    ShortcutConfig shortcuts() const;
    OrderedJson raw() const;
    const std::filesystem::path &filePath() const { return m_path; }

    void setTheme(AppTheme theme);
    void updatePanel(const OrderedJson &partial);
    void updateShortcut(const OrderedJson &partial);
    void updateKeyboard(const OrderedJson &partial);
    Result<void> replace(const OrderedJson &config);

    static OrderedJson defaults();
    static Config fromJson(const OrderedJson &json);
    static Result<void> validateShortcuts(const ShortcutConfig &shortcuts);
    static Result<OrderedJson> normalizeImported(const OrderedJson &current, const OrderedJson &imported);
    static Result<void> writeSettingsFile(const std::filesystem::path &file, const OrderedJson &config);
    static Result<OrderedJson> readSettingsFile(const std::filesystem::path &file);
    static double safeSpeed(const PanelConfig &panel);
    static double safeDelayNextMs(const PanelConfig &panel);

signals:
    void changed();

private:
    OrderedJson loadFromDisk();
    Result<void> persistLocked();
    void mergeSection(const char *section, const OrderedJson &partial);

    std::filesystem::path m_path;
    mutable std::mutex m_mutex;
    OrderedJson m_raw;
    Config m_config;
    std::atomic<double> m_speed{1.0};
};

}  // namespace sky
