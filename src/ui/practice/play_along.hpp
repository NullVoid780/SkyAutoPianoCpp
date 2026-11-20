#pragma once

#include <memory>
#include <vector>

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QRect>

#include "app/sky_window_watcher.hpp"
#include "model/key_map.hpp"
#include "services/practice/practice_session.hpp"
#include "ui/practice/sky_guide_stage.hpp"

class QTimer;

namespace skyinput {
class LowLevelHook;
}

namespace sky {

class ConfigService;
class PlayAlongBar;
class PlayAlongOverlay;
class AlignPrompt;
class SkyCalibrator;
struct SkyGrid;
class UiStateService;

class PlayAlong final : public QObject {
    Q_OBJECT

public:
    PlayAlong(ConfigService &config, UiStateService &uiState, QObject *parent = nullptr);
    ~PlayAlong() override;

    void start(const QString &name, const KeyMap &keyMap);
    void stop();
    bool isActive() const { return m_active; }
    void setSkyState(SkyWindowState state);

signals:
    void finished();

private:
    void tick();
    void restart();
    void press(int key);
    void place();
    void refreshVisibility();
    void installHooks();
    void removeHooks();
    void refreshLabels();
    void cycleSpeed();
    void cycleOpacity();
    void toggleMove();
    void toggleFloating();
    void toggleAlign();
    void finishAlign(bool ok, const SkyGrid &grid, const QString &message);
    void flashMessage(const QString &message);
    void placePrompt();
    QRect defaultFloatingRect(const QRect &anchor) const;
    void saveLayout();
    void loadLayout();
    QRect anchorRect() const;

    ConfigService &m_config;
    UiStateService &m_uiState;
    PracticeSession m_session;
    QPointer<PlayAlongOverlay> m_overlay;
    QPointer<PlayAlongBar> m_bar;
    QTimer *m_timer;
    QElapsedTimer m_clock;
    std::vector<std::unique_ptr<skyinput::LowLevelHook>> m_hooks;
    SkyGuideStage::Phase m_phase = SkyGuideStage::Phase::Countdown;
    SkyWindowState m_sky = SkyWindowState::NotRunning;
    bool m_active = false;
    bool m_shown = false;
    bool m_focusPaused = false;
    double m_last = 0;
    double m_countdown = 0;
    double m_speed = 1.0;
    double m_anchorX = 0.5;
    double m_anchorY = 0.376;
    double m_scaleX = 1.0;
    double m_scaleY = 1.0;
    SkyCalibrator *m_calibrator;
    QPointer<AlignPrompt> m_prompt;
    SkyGuideStage::Phase m_phaseBeforeAlign = SkyGuideStage::Phase::Playing;
    int m_opacity = 85;
    bool m_floating = false;
    QRect m_floatingRect;
    QRect m_lastAnchor;
};

}  // namespace sky
