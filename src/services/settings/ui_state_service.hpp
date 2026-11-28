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

    // Versioned legacy-migration state stored under "legacyMigration" in
    // ui-state.json. Status is one of: not_detected, available, importing,
    // needs_review, verified. A status left at "importing" by a crash is
    // downgraded to "needs_review" on load, never treated as success.
    OrderedJson legacyMigration() const;
    std::string legacyMigrationStatus() const;
    bool setLegacyMigration(const std::string &status, const std::string &sourceFingerprint,
                            OrderedJson result);

    // Separate record that the legacy Electron app was removed through the
    // Phase 2 flow. Stored apart from "legacyMigration" so the Phase 1
    // import evidence is never erased by a removal.
    bool legacyRemoved() const;
    void setLegacyRemoved();

    bool editorAdvancedView() const;
    void setEditorAdvancedView(bool advanced);

    bool editorColorfulNotes() const;
    void setEditorColorfulNotes(bool colorful);

    bool editorSoundMuted() const;
    void setEditorSoundMuted(bool muted);

    std::string language() const;
    void setLanguage(const std::string &code);
    static std::string readLanguage(const std::filesystem::path &dataDirectory);

    const std::filesystem::path &filePath() const { return m_path; }

signals:
    void themeChanged(sky::AppTheme theme);
    void favoritesChanged();
    void recentPlaysChanged();

private:
    bool save();

    std::filesystem::path m_path;
    OrderedJson m_raw = OrderedJson::object();
    AppTheme m_theme = AppTheme::Light;
    std::vector<std::string> m_favorites;
    std::vector<std::string> m_recentPlays;
};

}  // namespace sky
