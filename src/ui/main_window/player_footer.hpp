#pragma once

#include <QWidget>

#include "app/player_controller.hpp"

class QLabel;
class QSlider;

namespace sky {

class Badge;
class IconButton;
class InlineSwitch;
class LoopControl;
class ModeSwitch;
class SkyStatusChip;
class SpeedControl;
class ThemeToggle;
class TransportButton;

class PlayerFooter final : public QWidget {
    Q_OBJECT

public:
    static constexpr int kHeight = 172;
    static constexpr int kOverhang = 10;

    explicit PlayerFooter(QWidget *parent = nullptr);

    void showTarget(const QString &name, bool hold, int totalSeconds);
    void showEmpty();
    void setPlaying(bool playing);
    void setPosition(int seconds);
    void setShortcutLabels(const QString &previous, const QString &play, const QString &next);
    void setSpeed(double speed);
    void setLongPress(bool enabled);
    void setLoopMode(LoopMode mode);
    void setDelaySeconds(int seconds);
    bool isEditingSpeed() const;
    void setSkyStatus(const QString &text, int tone, const QString &tip);
    void setPracticeMode(bool practice);

signals:
    void settingsRequested();
    void themeToggleRequested();
    void previousRequested();
    void playRequested();
    void nextRequested();
    void seekRequested(int seconds);
    void longPressToggled(bool enabled);
    void speedCommitted(double speed);
    void loopCycleRequested();
    void delayChanged(int seconds);
    void practiceModeChosen(bool practice);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void relayout();
    void refreshTransportLabels();

    IconButton *m_gear;
    ThemeToggle *m_theme;
    QLabel *m_name;
    Badge *m_hold;
    QSlider *m_progress;
    QLabel *m_elapsed;
    QLabel *m_total;
    InlineSwitch *m_longPress;
    LoopControl *m_loop;
    TransportButton *m_previous;
    TransportButton *m_play;
    TransportButton *m_next;
    SpeedControl *m_speed;
    SkyStatusChip *m_sky;
    ModeSwitch *m_mode;
    bool m_practice = false;
    QString m_previousShortcut;
    QString m_playShortcut;
    QString m_nextShortcut;
    bool m_playing = false;
};

}  // namespace sky
