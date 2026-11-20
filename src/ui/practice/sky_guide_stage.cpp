#include "ui/practice/sky_guide_stage.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"

namespace sky {

namespace {

constexpr int kColumns = 5;
constexpr qreal kColumnStep = 0.10;
constexpr qreal kRowStep = 0.199;
constexpr qreal kIdleShare = 0.024;
constexpr qreal kActiveShare = 0.05;
constexpr qreal kUpcomingShrink = 0.94;
constexpr qreal kRadiusShare = 0.18;
constexpr qreal kLeadMs = 1500;
constexpr qreal kCurrentShare = 0.3;
constexpr int kUpcomingChords = 2;
constexpr qreal kSmoothMs = 70;
constexpr qreal kSparkleMs = 380;
constexpr qreal kFlashMs = 260;
constexpr qreal kMissMs = 320;
constexpr qreal kTravelMs = 420;
constexpr qreal kEffectLifeMs = 600;
constexpr qreal kResultsInMs = 280;
constexpr qreal kOrbitShare = 0.62;
constexpr qreal kOrbitSpeed = 0.0024;
constexpr qreal kOrbitStart = -2.356;
constexpr qreal kFitColumns = 5.6;
constexpr qreal kFitRows = 3.7;
constexpr qreal kFitIdle = 0.26;
constexpr qreal kFitActive = 0.54;
constexpr qreal kPanelRadius = 18;

const QColor kInk(255, 255, 255);
const QColor kFill(24, 15, 12);
const QColor kMiss(255, 110, 100);

qreal unit(qreal value)
{
    return std::clamp(value, 0.0, 1.0);
}

qreal easeOut(qreal t)
{
    t = unit(t);
    return 1.0 - std::pow(1.0 - t, 3.0);
}

qreal easeInOut(qreal t)
{
    t = unit(t);
    return t < 0.5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
}

QColor alpha(QColor color, qreal amount)
{
    color.setAlphaF(static_cast<float>(unit(amount)));
    return color;
}

QPainterPath sparkle(qreal outer)
{
    const qreal inner = outer * 0.16;
    QPainterPath path;
    for (int i = 0; i < 8; ++i) {
        const qreal angle = i * 3.14159265358979 / 4 - 3.14159265358979 / 2;
        const qreal radius = i % 2 == 0 ? outer : inner;
        const QPointF point(std::cos(angle) * radius, std::sin(angle) * radius);
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    return path;
}

void paintGlowStar(QPainter &painter, const QPointF &center, qreal size, qreal opacity)
{
    QRadialGradient glow(center, size * 1.4);
    glow.setColorAt(0.0, alpha(kInk, 0.55 * opacity));
    glow.setColorAt(1.0, alpha(kInk, 0.0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawEllipse(center, size * 1.4, size * 1.4);
    painter.save();
    painter.translate(center);
    painter.setBrush(alpha(kInk, opacity));
    painter.drawPath(sparkle(size));
    painter.restore();
}

}  // namespace

SkyGuideStage::SkyGuideStage(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    m_sizes.fill(0);
}

void SkyGuideStage::setGrid(double centerX, double centerY, double scaleX, double scaleY)
{
    m_centerX = centerX;
    m_centerY = centerY;
    m_scaleX = scaleX;
    m_scaleY = scaleY;
    update();
}

void SkyGuideStage::setFitMode(bool fit)
{
    m_fit = fit;
    update();
}

void SkyGuideStage::setMoveMode(bool moving)
{
    m_moving = moving;
    update();
}

void SkyGuideStage::clearEffects()
{
    m_effects.clear();
    m_trailFrom = -1;
    m_restKey = -1;
}

void SkyGuideStage::addHit(const PracticeHit &hit, double nowMs)
{
    if (hit.key < 0) {
        return;
    }
    m_effects.push_back(Effect{hit.key, hit.judgement, nowMs});
    if (hit.judgement == PracticeJudgement::Perfect || hit.judgement == PracticeJudgement::Good) {
        m_trailFrom = hit.key;
        m_trailStart = nowMs;
    }
}

qreal SkyGuideStage::fitStep() const
{
    return std::min(width() / kFitColumns, height() / kFitRows);
}

QPointF SkyGuideStage::orbitPoint(int key, qreal angle) const
{
    const qreal radius = activeSize() * kOrbitShare;
    return keyCenter(key) + QPointF(std::cos(angle) * radius, std::sin(angle) * radius);
}

QPointF SkyGuideStage::keyCenter(int key) const
{
    if (m_fit) {
        const qreal step = fitStep();
        return QPointF(width() / 2.0 + (key % kColumns - 2) * step, height() / 2.0 + (key / kColumns - 1) * step);
    }
    const qreal x = width() * m_centerX + (key % kColumns - 2) * width() * kColumnStep * m_scaleX;
    const qreal y = height() * m_centerY + (key / kColumns - 1) * height() * kRowStep * m_scaleY;
    return QPointF(x, y);
}

qreal SkyGuideStage::idleSize() const
{
    return m_fit ? fitStep() * kFitIdle : width() * kIdleShare * m_scaleX;
}

qreal SkyGuideStage::activeSize() const
{
    return m_fit ? fitStep() * kFitActive : width() * kActiveShare * m_scaleX;
}

std::array<SkyGuideStage::KeyState, kSkyKeyCount> SkyGuideStage::keyStates() const
{
    std::array<KeyState, kSkyKeyCount> states;
    states.fill(KeyState::Idle);
    if (!m_session || m_phase == Phase::Results) {
        return states;
    }
    const auto promote = [&states](int key, KeyState state) {
        if (static_cast<int>(state) > static_cast<int>(states[static_cast<std::size_t>(key)])) {
            states[static_cast<std::size_t>(key)] = state;
        }
    };
    const auto &notes = m_session->notes();
    const auto &chords = m_session->chords();
    if (m_session->waitMode()) {
        const int active = m_session->activeChord();
        if (active < 0) {
            return states;
        }
        for (int chord = active; chord < std::min<int>(active + 1 + kUpcomingChords, static_cast<int>(chords.size()));
             ++chord) {
            for (const int index : chords[static_cast<std::size_t>(chord)].notes) {
                const PracticeNote &note = notes[static_cast<std::size_t>(index)];
                if (note.state == PracticeNote::State::Pending) {
                    promote(note.key, chord == active ? KeyState::Current : KeyState::Upcoming);
                }
            }
        }
        return states;
    }
    const double lead = kLeadMs * m_speed;
    const double now = m_session->time();
    for (const PracticeNote &note : notes) {
        if (note.state != PracticeNote::State::Pending) {
            continue;
        }
        const double dt = note.time - now;
        if (dt > lead) {
            break;
        }
        if (dt < -PracticeSession::kGoodMs * m_speed) {
            continue;
        }
        promote(note.key, dt <= lead * kCurrentShare ? KeyState::Current : KeyState::Upcoming);
    }
    return states;
}

int SkyGuideStage::nextKey() const
{
    if (!m_session) {
        return -1;
    }
    for (const PracticeNote &note : m_session->notes()) {
        if (note.state == PracticeNote::State::Pending) {
            return note.key;
        }
    }
    return -1;
}

void SkyGuideStage::setFrame(Phase phase, double nowMs, double speed, double countdownMs)
{
    if (phase == Phase::Results && m_phase != Phase::Results) {
        m_resultsAt = nowMs;
    }
    m_phase = phase;
    m_speed = speed;
    m_countdown = countdownMs;
    const double delta = m_lastFrame < 0 ? 1000 : std::clamp(nowMs - m_lastFrame, 0.0, 100.0);
    m_lastFrame = nowMs;
    m_now = nowMs;
    const int upcoming = nextKey();
    if (upcoming != m_restKey) {
        m_restKey = upcoming;
        m_restStart = nowMs;
    }

    const auto states = keyStates();
    const qreal follow = 1.0 - std::exp(-delta / kSmoothMs);
    for (int key = 0; key < kSkyKeyCount; ++key) {
        const KeyState state = states[static_cast<std::size_t>(key)];
        const qreal target = state == KeyState::Idle       ? idleSize()
                             : state == KeyState::Upcoming ? activeSize() * kUpcomingShrink
                                                           : activeSize();
        qreal &size = m_sizes[static_cast<std::size_t>(key)];
        size = size <= 0 ? target : size + (target - size) * follow;
    }
    m_effects.erase(std::remove_if(m_effects.begin(), m_effects.end(),
                                   [nowMs](const Effect &effect) { return nowMs - effect.start > kEffectLifeMs; }),
                    m_effects.end());
    update();
}

void SkyGuideStage::paintKey(QPainter &painter, int key, KeyState state) const
{
    const QPointF center = keyCenter(key);
    const qreal size = std::max<qreal>(4, m_sizes[static_cast<std::size_t>(key)]);
    const qreal grow = unit((size - idleSize()) / std::max<qreal>(1, activeSize() - idleSize()));
    const QRectF body(center.x() - size / 2, center.y() - size / 2, size, size);
    const qreal radius = size * kRadiusShare;

    qreal flash = 0;
    qreal miss = 0;
    for (const Effect &effect : m_effects) {
        if (effect.key != key) {
            continue;
        }
        const double age = m_now - effect.start;
        if (effect.judgement == PracticeJudgement::Perfect || effect.judgement == PracticeJudgement::Good) {
            flash = std::max(flash, 1.0 - unit(age / kFlashMs));
        } else {
            miss = std::max(miss, 1.0 - unit(age / kMissMs));
        }
    }

    if (state == KeyState::Current) {
        for (int ring = 3; ring >= 1; --ring) {
            painter.setPen(QPen(alpha(kInk, 0.08 * ring), ring * 2.2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(body.adjusted(-ring, -ring, ring, ring), radius + ring, radius + ring);
        }
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(alpha(kFill, 0.38 + 0.14 * grow));
    painter.drawRoundedRect(body, radius, radius);
    if (flash > 0) {
        painter.setBrush(alpha(kInk, 0.42 * flash));
        painter.drawRoundedRect(body, radius, radius);
    }

    QColor border = alpha(kInk, 0.12 + 0.45 * grow);
    qreal borderWidth = 1.0 + 0.4 * grow;
    if (state == KeyState::Current) {
        border = alpha(kInk, 0.95);
        borderWidth = std::max<qreal>(2.0, size * 0.045);
    }
    if (miss > 0) {
        border = alpha(kMiss, 0.4 + 0.6 * miss);
        borderWidth = std::max(borderWidth, size * 0.04);
    }
    const qreal inset = size * 0.1 * grow;
    painter.setPen(QPen(border, borderWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body.adjusted(inset, inset, -inset, -inset), radius * (1 - 0.3 * grow), radius * (1 - 0.3 * grow));

    const QString label = m_labels[static_cast<std::size_t>(key)];
    if (!label.isEmpty()) {
        painter.setFont(FontLoader::font(std::max(7, static_cast<int>(size * (0.26 + 0.06 * grow))), QFont::DemiBold));
        painter.setPen(alpha(kInk, 0.7 + 0.3 * grow));
        painter.drawText(body, Qt::AlignCenter, label);
    }
}

void SkyGuideStage::paintTrail(QPainter &painter) const
{
    if (m_phase == Phase::Results) {
        return;
    }
    const int target = nextKey();
    if (target < 0) {
        return;
    }
    const qreal starSize = activeSize() * 0.22;
    const qreal travel = easeInOut((m_now - m_trailStart) / kTravelMs);
    if (m_trailFrom < 0 || target == m_trailFrom || travel >= 1.0) {
        const double since = m_trailFrom < 0 || target == m_trailFrom ? m_now - m_restStart : m_now - m_trailStart - kTravelMs;
        const qreal angle = kOrbitStart + std::max(0.0, since) * kOrbitSpeed;
        const qreal pulse = 0.85 + 0.15 * std::sin(m_now * 0.01);
        paintGlowStar(painter, orbitPoint(target, angle), starSize * pulse, 0.95);
        return;
    }
    const QPointF from = keyCenter(m_trailFrom);
    const QPointF to = orbitPoint(target, kOrbitStart);
    const QPointF middle = (from + to) / 2;
    const QPointF direction = to - from;
    const qreal distance = std::hypot(direction.x(), direction.y());
    if (distance < 1) {
        return;
    }
    QPointF normal(-direction.y() / distance, direction.x() / distance);
    if (normal.y() > 0) {
        normal = -normal;
    }
    const QPointF control = middle + normal * distance * 0.35;
    QPainterPath arc(from);
    arc.quadTo(control, to);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(alpha(kInk, 0.55), std::max<qreal>(1.2, activeSize() * 0.022), Qt::SolidLine, Qt::RoundCap));
    painter.drawPath(arc);
    paintGlowStar(painter, arc.pointAtPercent(travel), starSize, 0.95);
}

void SkyGuideStage::paintEffects(QPainter &painter) const
{
    for (const Effect &effect : m_effects) {
        if (effect.judgement != PracticeJudgement::Perfect && effect.judgement != PracticeJudgement::Good) {
            continue;
        }
        const qreal t = unit((m_now - effect.start) / kSparkleMs);
        const qreal size = activeSize() * (0.45 + 0.55 * easeOut(t));
        paintGlowStar(painter, keyCenter(effect.key), size * 0.5, 1.0 - t * t);
    }
}

void SkyGuideStage::paintCenterText(QPainter &painter) const
{
    QString text;
    qreal scale = 1.0;
    qreal opacity = 1.0;
    if (m_phase == Phase::Countdown && m_countdown > 0) {
        const qreal elapsed = 1.0 - std::fmod(m_countdown, 1000.0) / 1000.0;
        text = QString::number(std::max(1, static_cast<int>(std::ceil(m_countdown / 1000.0))));
        scale = 1.4 - 0.4 * easeOut(elapsed * 1.5);
        opacity = 1.0 - std::pow(elapsed, 4.0);
    } else if (m_phase == Phase::Paused) {
        text = QStringLiteral("Paused");
    } else if (m_moving) {
        return;
    } else {
        return;
    }
    const qreal rowStep = m_fit ? fitStep() : height() * kRowStep * m_scaleY;
    const QPointF anchor(keyCenter(2).x(), keyCenter(0).y() - rowStep * 0.62);
    painter.save();
    painter.translate(anchor);
    painter.scale(scale, scale);
    painter.setOpacity(opacity);
    const int fontSize = std::max(16, static_cast<int>(m_fit ? fitStep() * 0.32 : width() * 0.03 * m_scaleX));
    painter.setFont(FontLoader::font(fontSize, QFont::Bold));
    const QRectF box(-200, -fontSize, 400, fontSize * 2);
    painter.setPen(alpha(QColor(0, 0, 0), 0.45));
    painter.drawText(box.translated(1.5, 2), Qt::AlignCenter, text);
    painter.setPen(kInk);
    painter.drawText(box, Qt::AlignCenter, text);
    painter.restore();
}

void SkyGuideStage::paintResults(QPainter &painter) const
{
    if (m_phase != Phase::Results || !m_session) {
        return;
    }
    const PracticeStats &stats = m_session->stats();
    const qreal t = easeOut(unit((m_now - m_resultsAt) / kResultsInMs));
    const QPointF center = keyCenter(7);
    const QSizeF size(360, 220);
    const qreal fit = std::min(1.0, (width() - 32) / size.width());
    painter.save();
    painter.setOpacity(t);
    painter.translate(center);
    painter.scale((0.92 + 0.08 * t) * fit, (0.92 + 0.08 * t) * fit);
    const QRectF card(-size.width() / 2, -size.height() / 2, size.width(), size.height());
    painter.setPen(QPen(alpha(kInk, 0.35), 1));
    painter.setBrush(alpha(kFill, 0.72));
    painter.drawRoundedRect(card, 16, 16);
    painter.setFont(FontLoader::font(56, QFont::Bold));
    painter.setPen(kInk);
    painter.drawText(QRectF(card.left(), card.top() + 12, card.width(), 70), Qt::AlignCenter,
                     QString::fromStdString(stats.grade()));
    painter.setFont(FontLoader::font(17, QFont::DemiBold));
    painter.drawText(QRectF(card.left(), card.top() + 86, card.width(), 26), Qt::AlignCenter,
                     QStringLiteral("%1% accuracy").arg(stats.accuracy(), 0, 'f', 1));
    painter.setFont(FontLoader::font(13, QFont::Normal));
    painter.setPen(alpha(kInk, 0.85));
    painter.drawText(QRectF(card.left(), card.top() + 124, card.width(), 22), Qt::AlignCenter,
                     QStringLiteral("Perfect %1     Good %2     Miss %3     Best combo %4")
                         .arg(stats.perfect)
                         .arg(stats.good)
                         .arg(stats.miss)
                         .arg(stats.bestCombo));
    painter.setPen(alpha(kInk, 0.6));
    painter.drawText(QRectF(card.left(), card.bottom() - 46, card.width(), 24), Qt::AlignCenter,
                     QStringLiteral("Press Restart, or Next for another song"));
    painter.restore();
}

void SkyGuideStage::paintMoveHint(QPainter &painter) const
{
    if (!m_moving) {
        return;
    }
    if (m_fit) {
        const QRectF label(width() / 2.0 - 170, height() - 40, 340, 28);
        painter.setPen(Qt::NoPen);
        painter.setBrush(alpha(kFill, 0.8));
        painter.drawRoundedRect(label, 9, 9);
        painter.setFont(FontLoader::font(12, QFont::DemiBold));
        painter.setPen(kInk);
        painter.drawText(label, Qt::AlignCenter, QStringLiteral("Drag to move, drag the edges to resize"));
        return;
    }
    const QPointF topLeft = keyCenter(0);
    const QPointF bottomRight = keyCenter(kSkyKeyCount - 1);
    const qreal pad = activeSize();
    const QRectF grid(topLeft - QPointF(pad, pad), bottomRight + QPointF(pad, pad));
    painter.setPen(QPen(alpha(kInk, 0.8), 2, Qt::DashLine));
    painter.setBrush(alpha(kFill, 0.15));
    painter.drawRoundedRect(grid, 14, 14);
    const QRectF label(grid.center().x() - 220, grid.bottom() + 12, 440, 32);
    painter.setPen(Qt::NoPen);
    painter.setBrush(alpha(kFill, 0.75));
    painter.drawRoundedRect(label, 10, 10);
    painter.setFont(FontLoader::font(13, QFont::DemiBold));
    painter.setPen(kInk);
    painter.drawText(label, Qt::AlignCenter, QStringLiteral("Drag to line up with Sky's keys, scroll to resize"));
}

void SkyGuideStage::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    preparePainter(painter);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (m_fit) {
        painter.setPen(QPen(alpha(kInk, m_moving ? 0.7 : 0.18), m_moving ? 2 : 1, m_moving ? Qt::DashLine : Qt::SolidLine));
        painter.setBrush(alpha(kFill, 0.42));
        painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), kPanelRadius, kPanelRadius);
    }
    paintMoveHint(painter);
    const auto states = keyStates();
    for (int key = 0; key < kSkyKeyCount; ++key) {
        paintKey(painter, key, states[static_cast<std::size_t>(key)]);
    }
    paintTrail(painter);
    paintEffects(painter);
    paintCenterText(painter);
    paintResults(painter);
}

}  // namespace sky
