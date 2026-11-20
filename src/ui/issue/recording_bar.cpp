#include "ui/issue/recording_bar.hpp"

#include <QAbstractButton>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QVariantAnimation>

#include <cmath>
#include <numbers>

#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"

namespace sky {

namespace {

constexpr int kShadow = 14;
constexpr int kHeight = 50;
constexpr int kWidth = 330;
constexpr int kTopGap = 12;
constexpr int kRise = 10;
const QColor kFill(24, 25, 30, 245);
const QColor kBorder(255, 255, 255, 22);
const QColor kText(244, 245, 248);
const QColor kMuted(160, 164, 176);
const QColor kRed(239, 68, 80);

QString clockText(qint64 milliseconds)
{
    const qint64 seconds = milliseconds / 1000;
    return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

class HudButton final : public QAbstractButton {
public:
    enum class Kind { Finish, Discard };

    HudButton(Kind kind, QWidget *parent) : QAbstractButton(parent), m_kind(kind)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setFixedSize(kind == Kind::Finish ? QSize(92, 34) : QSize(34, 34));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        preparePainter(painter);
        const QRectF area = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const bool hot = underMouse();
        const bool down = isDown();
        if (m_kind == Kind::Finish) {
            QColor fill = kRed;
            if (down) {
                fill = fill.darker(115);
            } else if (hot) {
                fill = fill.lighter(112);
            }
            painter.fillPath(roundedPath(area, area.height() / 2), fill);
            const QRectF square(14, area.center().y() - 5, 10, 10);
            painter.fillPath(roundedPath(square, 2.5), Qt::white);
            QFont font = painter.font();
            font.setPixelSize(13);
            font.setWeight(QFont::DemiBold);
            painter.setFont(font);
            painter.setPen(Qt::white);
            painter.drawText(area.adjusted(30, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("Finish"));
            return;
        }
        if (hot || down) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, down ? 34 : 22));
            painter.drawEllipse(area);
        }
        const QPixmap icon = IconCache::pixmap(QStringLiteral("trash3"), 15, hot ? kText : kMuted, devicePixelRatioF());
        const QSizeF size = QSizeF(icon.size()) / icon.devicePixelRatio();
        painter.drawPixmap(QPointF(area.center().x() - size.width() / 2, area.center().y() - size.height() / 2), icon);
    }

private:
    Kind m_kind;
};

RecordingBar::RecordingBar()
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
      m_finish(new HudButton(HudButton::Kind::Finish, this)),
      m_discard(new HudButton(HudButton::Kind::Discard, this)),
      m_pulse(new QVariantAnimation(this)),
      m_appear(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowTitle(QStringLiteral("Recording an issue"));
    setFixedSize(kWidth + kShadow * 2, kHeight + kShadow * 2);
    setCursor(Qt::SizeAllCursor);
    m_finish->setToolTip(QStringLiteral("Stop recording and save the report"));
    m_finish->setAccessibleName(QStringLiteral("Finish recording"));
    m_discard->setToolTip(QStringLiteral("Throw away this recording"));
    m_discard->setAccessibleName(QStringLiteral("Discard recording"));
    layoutButtons();

    m_pulse->setStartValue(0.0);
    m_pulse->setEndValue(1.0);
    m_pulse->setDuration(1600);
    m_pulse->setLoopCount(-1);
    connect(m_pulse, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_pulseValue = value.toReal();
        update();
    });
    m_pulse->start();

    m_appear->setStartValue(0.0);
    m_appear->setEndValue(1.0);
    m_appear->setDuration(240);
    m_appear->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_appear, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_appearValue = value.toReal();
        setWindowOpacity(m_appearValue);
        update();
    });

    connect(m_finish, &QAbstractButton::clicked, this, &RecordingBar::stopRequested);
    connect(m_discard, &QAbstractButton::clicked, this, &RecordingBar::cancelRequested);
}

void RecordingBar::layoutButtons()
{
    const int centerY = kShadow + kHeight / 2;
    m_discard->move(kShadow + kWidth - 8 - m_discard->width(), centerY - m_discard->height() / 2);
    m_finish->move(m_discard->x() - 4 - m_finish->width(), centerY - m_finish->height() / 2);
}

void RecordingBar::setElapsed(qint64 milliseconds)
{
    m_elapsed = milliseconds;
    update();
}

void RecordingBar::setSteps(int steps)
{
    m_steps = steps;
    update();
}

void RecordingBar::placeNear(QWidget *window)
{
    QScreen *screen = window ? window->screen() : nullptr;
    const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
    move(area.center().x() - width() / 2, area.top() + kTopGap - kShadow + kRise);
}

void RecordingBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    const QPoint target = pos() - QPoint(0, kRise);
    auto *slide = new QVariantAnimation(this);
    slide->setStartValue(pos());
    slide->setEndValue(target);
    slide->setDuration(240);
    slide->setEasingCurve(QEasingCurve::OutCubic);
    connect(slide, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        if (!m_dragging) {
            move(value.toPoint());
        }
    });
    slide->start(QAbstractAnimation::DeleteWhenStopped);
    m_appear->start();
}

void RecordingBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    preparePainter(painter);
    const QRectF pill(kShadow, kShadow, kWidth, kHeight);
    const qreal radius = pill.height() / 2;
    painter.setPen(Qt::NoPen);
    for (int i = kShadow; i > 0; i -= 2) {
        const qreal fade = 1.0 - static_cast<qreal>(i) / kShadow;
        painter.setBrush(QColor(0, 0, 0, static_cast<int>(16 * fade * fade + 2)));
        painter.drawRoundedRect(pill.adjusted(-i, -i + 3, i, i + 3), radius + i, radius + i);
    }
    painter.fillPath(roundedPath(pill, radius), kFill);
    painter.setPen(QPen(kBorder, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(roundedPath(pill.adjusted(0.5, 0.5, -0.5, -0.5), radius - 0.5));

    const QPointF dot(pill.left() + 25, pill.center().y());
    QColor ring = kRed;
    ring.setAlphaF(static_cast<float>(0.5 * (1.0 - m_pulseValue)));
    painter.setPen(Qt::NoPen);
    painter.setBrush(ring);
    const qreal ringRadius = 5.5 + 7.5 * m_pulseValue;
    painter.drawEllipse(dot, ringRadius, ringRadius);
    const qreal breathe = 0.85 + 0.15 * std::cos(m_pulseValue * 2 * std::numbers::pi);
    painter.setBrush(kRed);
    painter.drawEllipse(dot, 5.5 * breathe, 5.5 * breathe);

    QFont label = font();
    label.setPixelSize(11);
    label.setWeight(QFont::Bold);
    label.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    painter.setFont(label);
    painter.setPen(kRed.lighter(125));
    const QRectF recArea(pill.left() + 40, pill.top(), 34, pill.height());
    painter.drawText(recArea, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("REC"));

    QFont time = font();
    time.setPixelSize(16);
    time.setWeight(QFont::DemiBold);
    time.setFeature(QFont::Tag("tnum"), 1);
    painter.setFont(time);
    painter.setPen(kText);
    const QRectF timeArea(recArea.right() + 2, pill.top(), 56, pill.height());
    painter.drawText(timeArea, Qt::AlignVCenter | Qt::AlignLeft, clockText(m_elapsed));

    painter.setPen(QPen(QColor(255, 255, 255, 26), 1));
    const qreal split = timeArea.right() + 6;
    painter.drawLine(QPointF(split, pill.top() + 15), QPointF(split, pill.bottom() - 15));

    QFont steps = font();
    steps.setPixelSize(12);
    painter.setFont(steps);
    painter.setPen(kMuted);
    const QRectF stepsArea(split + 10, pill.top(), m_finish->x() - split - 14, pill.height());
    painter.drawText(stepsArea, Qt::AlignVCenter | Qt::AlignLeft,
                     m_steps == 1 ? QStringLiteral("1 step") : QStringLiteral("%1 steps").arg(m_steps));
}

void RecordingBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - pos();
    }
}

void RecordingBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
}

void RecordingBar::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}

}  // namespace sky
