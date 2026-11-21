#pragma once

#include <QPoint>
#include <QWidget>

class QLabel;

namespace sky {

class ActionButton;

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
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QLabel *m_time;
    QLabel *m_steps;
    ActionButton *m_stop;
    ActionButton *m_discard;
    QPoint m_dragOffset;
    bool m_dragging = false;
};

}  // namespace sky
