#pragma once

#include <QWidget>

#include "model/loop_mode.hpp"

class QLabel;
class QSlider;

namespace sky {

class HoverTracker;

class LoopButton final : public QWidget {
    Q_OBJECT

public:
    explicit LoopButton(QWidget *parent = nullptr);

    void setMode(LoopMode mode);
    QSize sizeHint() const override { return QSize(50, 16); }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    LoopMode m_mode = LoopMode::Off;
    HoverTracker *m_hover;
};

class LoopControl final : public QWidget {
    Q_OBJECT

public:
    explicit LoopControl(QWidget *parent = nullptr);

    void setMode(LoopMode mode);
    void setDelaySeconds(int seconds);

signals:
    void cycleRequested();
    void delayChanged(int seconds);

private:
    LoopButton *m_button;
    QSlider *m_slider;
    QLabel *m_caption;
};

}  // namespace sky
