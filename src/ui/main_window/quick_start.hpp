#pragma once

#include <vector>

#include "ui/core/overlay.hpp"

class QLabel;
class QStackedWidget;

namespace sky {

class ActionButton;
class ChoiceTile;
class ConfigService;
class SelectBox;
class StepDots;
class ThemeChip;
class ThemeController;
class ToggleSwitch;

class QuickStart final : public Overlay {
    Q_OBJECT

public:
    enum class LegacyState { Searching, Found, NotFound, UpToDate, Importing, Imported };

    QuickStart(ThemeController &themes, ConfigService &config, QWidget *window);

    void start();
    void setLegacyState(LegacyState state, int songs = 0, int playlists = 0, const QString &detail = {});
    LegacyState legacyState() const { return m_legacyState; }
    // The same legacy cleanup state the Settings legacy page shows: the
    // application computes it once and pushes it to both surfaces. The
    // cleanup lives on its own step, present only while a legacy
    // installation or migration state exists.
    void setLegacyCleanup(const QString &description, bool removeEnabled, bool busy, bool present);
    bool isLegacyCleanupShown() const;
    // The same language preference the Settings > General tab edits: the
    // application seeds it from the persisted value, and every change is
    // reported back through languageChosen so it is stored in the same place.
    void setLanguage(const QString &preference);

signals:
    void finished();
    void legacyImportRequested();
    void legacyFolderRequested();
    void legacyRemovalRequested();
    void legacyCleanupShown();
    void languageChosen(const QString &preference);

protected:
    QWidget *panel() const override { return m_panel; }
    QColor dimColor() const override;
    ShadowSpec panelShadow() const override;
    qreal panelRadius() const override { return 20; }
    QRect panelGeometry() const override;
    void paintPanel(QPainter &painter, const QRectF &rect) override;

private:
    QWidget *buildWelcome();
    QWidget *buildLook();
    QWidget *buildLegacy();
    QWidget *buildCleanup();
    QWidget *buildPlayback();
    QWidget *buildControls();
    QWidget *buildDone();
    QWidget *page(const QString &iconId, const QString &title, const QString &body);
    void showStep(int index);
    void next();
    void back();
    void finish();
    void syncLook();
    void loadControls();
    void applyCleanup();
    void buildSteps();
    void rebuildSteps();
    void syncControls();
    void retranslateChrome();
    void onLanguageIndexChanged(int index);

    enum class Step { Welcome, Look, Legacy, Cleanup, Playback, Controls, Done };

    ThemeController &m_themes;
    ConfigService &m_config;
    QWidget *m_panel;
    QStackedWidget *m_stack;
    std::vector<QWidget *> m_steps;
    std::vector<Step> m_stepRoles;
    QWidget *m_welcome;
    QWidget *m_look;
    QWidget *m_legacy;
    QWidget *m_cleanupPage = nullptr;
    QWidget *m_playback;
    QWidget *m_controls;
    QWidget *m_done;
    int m_current = 0;
    LegacyState m_legacyState = LegacyState::Searching;
    bool m_finished = false;
    QLabel *m_cleanupDescription = nullptr;
    ActionButton *m_cleanupRemove = nullptr;
    QString m_cleanupText;
    bool m_cleanupKnown = false;
    bool m_cleanupRemoveEnabled = false;
    bool m_cleanupBusy = false;
    bool m_cleanupStepWanted = false;
    SelectBox *m_language = nullptr;
    QString m_languagePreference;
    int m_legacySongs = 0;
    int m_legacyPlaylists = 0;
    QString m_legacyDetail;
    QLabel *m_legacyBody = nullptr;
    QLabel *m_legacyNote = nullptr;
    QWidget *m_legacyChoices = nullptr;
    ActionButton *m_folderButton = nullptr;
    ChoiceTile *m_importTile = nullptr;
    std::vector<ChoiceTile *> m_modeTiles;
    std::vector<ThemeChip *> m_themeChips;
    ChoiceTile *m_liteTile = nullptr;
    ChoiceTile *m_strictTile = nullptr;
    ToggleSwitch *m_minimize = nullptr;
    std::vector<QLabel *> m_shortcutKeys;
    StepDots *m_dots;
    ActionButton *m_back;
    ActionButton *m_skip;
    ActionButton *m_next;
};

}  // namespace sky
