#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <QObject>

#include "core/json.hpp"
#include "model/config.hpp"

namespace sky {

class UiStateService final : public QObject {
    Q_OBJECT

public:
    explicit UiStateService(std::filesystem::path dataDirectory, QObject *parent = nullptr);

    void load();

    AppTheme theme() const { return m_theme; }
    void setTheme(AppTheme theme);

    std::string themeId() const;
    void setThemeId(const std::string &id);

    bool quickStartDone() const;
    void setQuickStartDone();

    bool followSystemTheme() const;
    void setFollowSystemTheme(bool follow);

    const std::vector<std::string> &favorites() const { return m_favorites; }
    void setFavorites(std::vector<std::string> names);

    const std::vector<std::string> &recentPlays() const { return m_recentPlays; }
    void setRecentPlays(std::vector<std::string> names);

    bool legacyImportOffered() const;
    void setLegacyImportOffered();

    OrderedJson value(const std::string &key) const;
    void setValue(const std::string &key, OrderedJson value);

    const std::filesystem::path &filePath() const { return m_path; }

signals:
    void themeChanged(sky::AppTheme theme);
    void favoritesChanged();
    void recentPlaysChanged();

private:
    void save();

    std::filesystem::path m_path;
    OrderedJson m_raw = OrderedJson::object();
    AppTheme m_theme = AppTheme::Light;
    std::vector<std::string> m_favorites;
    std::vector<std::string> m_recentPlays;
};

}  // namespace sky
