#pragma once

#include <QPoint>
#include <QWidget>

class QVariantAnimation;

namespace sky {

class HudButton;

class RecordingBar final : public QWidget {
    Q_OBJECT

public:
    RecordingBar();

    void setElapsed(qint64 milliseconds);
    void setSteps(int steps);
    void placeNear(QWidget *window);

signals:
    void stopRequested();
    void cancelRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void layoutButtons();

    HudButton *m_finish;
    HudButton *m_discard;
    QVariantAnimation *m_pulse;
    QVariantAnimation *m_appear;
    qint64 m_elapsed = 0;
    int m_steps = 0;
    qreal m_pulseValue = 0;
    qreal m_appearValue = 1;
    QPoint m_dragOffset;
    bool m_dragging = false;
};

}  // namespace sky
