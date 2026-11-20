#pragma once

#include <optional>

#include <QImage>
#include <QObject>
#include <QPointF>
#include <QRect>
#include <QRectF>

class QTimer;

namespace sky {

struct SkyGrid {
    double centerX = 0.5;
    double centerY = 0.376;
    double scaleX = 1.0;
    double scaleY = 1.0;
};

std::optional<QPointF> locateFlash(const QImage &before, const QImage &after, const QRectF &searchArea);
std::optional<SkyGrid> gridFromCorners(const QPointF &topLeftKey, const QPointF &bottomRightKey, const QSizeF &window);

class SkyCalibrator final : public QObject {
    Q_OBJECT

public:
    explicit SkyCalibrator(QObject *parent = nullptr);

    bool isRunning() const { return m_step != Step::Idle; }
    void start(const QRect &skyRect);
    void cancel();
    void keyPressed(int key);

signals:
    void instruction(const QString &text);
    void prompt(int step, const QString &key, const QString &note);
    void finished(bool ok, const sky::SkyGrid &grid, const QString &message);

private:
    enum class Step { Idle, TopLeft, BottomRight, Measuring };

    QImage capture() const;
    void measure();
    void fail(const QString &message);

    QTimer *m_poll;
    QTimer *m_timeout;
    QRect m_rect;
    Step m_step = Step::Idle;
    Step m_measuring = Step::Idle;
    QImage m_latest;
    QImage m_before;
    QImage m_best;
    int m_shots = 0;
    std::optional<QPointF> m_topLeft;
};

}  // namespace sky
