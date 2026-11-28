#pragma once

#include <vector>

#include "ui/core/overlay.hpp"

class QLabel;
class QStackedWidget;

namespace sky {

class ActionButton;
class ChoiceTile;
class ConfigService;
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
    // The same legacy cleanup state the Settings > General section shows:
    // the application computes it once and pushes it to both surfaces.
    void setLegacyCleanup(const QString &description, bool removeEnabled, bool verifyVisible, bool busy);

signals:
    void finished();
    void legacyImportRequested();
    void legacyFolderRequested();
    void legacyRemovalRequested();
    void legacyVerifyRequested();

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

    ThemeController &m_themes;
    ConfigService &m_config;
    QWidget *m_panel;
    QStackedWidget *m_stack;
    std::vector<QWidget *> m_steps;
    QWidget *m_welcome;
    QWidget *m_look;
    QWidget *m_legacy;
    QWidget *m_playback;
    QWidget *m_controls;
    QWidget *m_done;
    int m_current = 0;
    LegacyState m_legacyState = LegacyState::Searching;
    bool m_finished = false;
    QLabel *m_cleanupDescription = nullptr;
    ActionButton *m_cleanupVerify = nullptr;
    ActionButton *m_cleanupRemove = nullptr;
    QString m_cleanupText;
    bool m_cleanupKnown = false;
    bool m_cleanupRemoveEnabled = false;
    bool m_cleanupVerifyVisible = false;
    bool m_cleanupBusy = false;
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
