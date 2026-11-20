#pragma once

#include <QPoint>
#include <QRect>
#include <QWidget>

class QLabel;

namespace sky {

class SkyGuideStage;

class PlayAlongOverlay final : public QWidget {
    Q_OBJECT

public:
    PlayAlongOverlay();

    SkyGuideStage *stage() const { return m_stage; }
    void setMoveMode(bool moving);
    bool isMoving() const { return m_moving; }
    void setFloating(bool floating) { m_floating = floating; }

signals:
    void dragged(const QPoint &delta);
    void windowEdited(const QRect &geometry);
    void windowEditFinished();
    void wheelResized(int steps);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Qt::WindowFlags flagsFor(bool moving) const;
    int edgesAt(const QPoint &point) const;
    void updateCursor(int edges);

    SkyGuideStage *m_stage;
    bool m_moving = false;
    bool m_dragging = false;
    bool m_floating = false;
    int m_edges = 0;
    QPoint m_last;
    QRect m_startGeometry;
    QPoint m_startPoint;
};

class AlignPrompt final : public QWidget {
    Q_OBJECT

public:
    AlignPrompt();

    void showStep(int step, const QString &key, const QString &note);
    void showResult(bool ok, const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_step = 0;
    QString m_key;
    QString m_note;
    QString m_result;
    bool m_ok = false;
    bool m_resultMode = false;
};

class ActionButton;
class ToggleSwitch;

class PlayAlongBar final : public QWidget {
    Q_OBJECT

public:
    PlayAlongBar();

    void setTitle(const QString &title);
    void setWait(bool wait);
    void setSpeed(double speed);
    void setOpacityPercent(int percent);
    void setMoving(bool moving);
    void setFloating(bool floating);
    void setMessage(const QString &message);
    void setAligning(bool aligning);

signals:
    void waitToggled(bool wait);
    void speedClicked();
    void opacityClicked();
    void moveClicked();
    void floatingClicked();
    void alignClicked();
    void restartClicked();
    void stopClicked();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *m_title;
    ToggleSwitch *m_wait;
    ActionButton *m_speed;
    ActionButton *m_opacity;
    ActionButton *m_move;
    ActionButton *m_floating;
    ActionButton *m_align;
    QString m_titleText;
    ActionButton *m_restart;
    ActionButton *m_stop;
};

}  // namespace sky
