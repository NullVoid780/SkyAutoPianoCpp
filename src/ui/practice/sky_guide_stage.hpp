#pragma once

#include <array>
#include <vector>

#include <QPointF>
#include <QString>
#include <QWidget>

#include "model/key_table.hpp"
#include "services/practice/practice_session.hpp"

namespace sky {

class SkyGuideStage final : public QWidget {
    Q_OBJECT

public:
    enum class Phase { Countdown, Playing, Paused, Results };

    explicit SkyGuideStage(QWidget *parent = nullptr);

    void setSession(const PracticeSession *session) { m_session = session; }
    void setLabels(const std::array<QString, kSkyKeyCount> &labels) { m_labels = labels; }
    void setGrid(double centerX, double centerY, double scaleX, double scaleY);
    void setMoveMode(bool moving);
    void setFitMode(bool fit);
    void setFrame(Phase phase, double nowMs, double speed, double countdownMs);
    void addHit(const PracticeHit &hit, double nowMs);
    void clearEffects();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    enum class KeyState { Idle, Upcoming, Current };

    struct Effect {
        int key = 0;
        PracticeJudgement judgement = PracticeJudgement::Wrong;
        double start = 0;
    };

    QPointF keyCenter(int key) const;
    qreal fitStep() const;
    QPointF orbitPoint(int key, qreal angle) const;
    qreal idleSize() const;
    qreal activeSize() const;
    std::array<KeyState, kSkyKeyCount> keyStates() const;
    int nextKey() const;
    void paintKey(QPainter &painter, int key, KeyState state) const;
    void paintTrail(QPainter &painter) const;
    void paintEffects(QPainter &painter) const;
    void paintCenterText(QPainter &painter) const;
    void paintResults(QPainter &painter) const;
    void paintMoveHint(QPainter &painter) const;

    const PracticeSession *m_session = nullptr;
    std::array<QString, kSkyKeyCount> m_labels;
    std::array<qreal, kSkyKeyCount> m_sizes{};
    std::vector<Effect> m_effects;
    Phase m_phase = Phase::Countdown;
    double m_now = 0;
    double m_lastFrame = -1;
    double m_speed = 1.0;
    double m_countdown = 0;
    double m_resultsAt = -1;
    double m_centerX = 0.5;
    double m_centerY = 0.376;
    double m_scaleX = 1.0;
    double m_scaleY = 1.0;
    int m_trailFrom = -1;
    double m_trailStart = 0;
    bool m_moving = false;
    bool m_fit = false;
    int m_restKey = -1;
    double m_restStart = 0;
};

}  // namespace sky
