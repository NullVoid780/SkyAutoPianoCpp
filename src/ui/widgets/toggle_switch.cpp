#include "ui/widgets/toggle_switch.hpp"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QSignalBlocker>
#include <QVariantAnimation>

#include "ui/core/easing.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kTrackWidth = 60;
constexpr qreal kTrackHeight = 30;
constexpr qreal kBorder = 2;
constexpr qreal kKnob = 22;
constexpr qreal kKnobInset = 4;
constexpr qreal kTravel = 30;
constexpr int kMargin = 6;

}  // namespace

ToggleSwitch::ToggleSwitch(QWidget *parent) : QAbstractButton(parent), m_slide(new QVariantAnimation(this))
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(sizeHint());
    m_slide->setDuration(kHoverSlowMs);
    m_slide->setEasingCurve(easingCurve(Easing::Ease));
    connect(m_slide, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_position = value.toReal();
        update();
    });
    connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        m_slide->stop();
        m_slide->setStartValue(m_position);
        m_slide->setEndValue(checked ? 1.0 : 0.0);
        m_slide->start();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(static_cast<int>(kTrackWidth) + kMargin * 2, static_cast<int>(kTrackHeight) + kMargin * 2);
}

void ToggleSwitch::setCheckedImmediately(bool checked)
{
    {
        const QSignalBlocker blocker(this);
        setChecked(checked);
    }
    m_slide->stop();
    m_position = checked ? 1.0 : 0.0;
    update();
}

QRectF ToggleSwitch::trackRect() const
{
    return QRectF(kMargin, kMargin, kTrackWidth, kTrackHeight);
}

bool ToggleSwitch::hitButton(const QPoint &pos) const
{
    return trackRect().adjusted(-kMargin, -kMargin, kMargin, kMargin).contains(pos);
}

void ToggleSwitch::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        click();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void ToggleSwitch::focusInEvent(QFocusEvent *event)
{
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    QAbstractButton::focusInEvent(event);
}

void ToggleSwitch::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus = false;
    QAbstractButton::focusOutEvent(event);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    QPainter painter(this);
    preparePainter(painter);
    const QRectF track = trackRect();
    const qreal radius = kTrackHeight / 2;
    const QRectF stroke = track.adjusted(kBorder / 2, kBorder / 2, -kBorder / 2, -kBorder / 2);

    painter.setOpacity(isEnabled() ? 1.0 : 0.5);
    painter.fillPath(roundedPath(track, radius), s.trackFill);
    painter.setPen(QPen(s.trackBorder, kBorder));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(stroke, radius - kBorder / 2, radius - kBorder / 2);
    if (m_position > 0) {
        painter.save();
        painter.setOpacity(painter.opacity() * m_position);
        painter.fillPath(roundedPath(track, radius), theme.hoverColor);
        painter.setPen(QPen(s.trackOnBorder, kBorder));
        painter.drawRoundedRect(stroke, radius - kBorder / 2, radius - kBorder / 2);
        painter.restore();
    }

    const QRectF knob(track.left() + kKnobInset + kTravel * m_position, track.top() + kKnobInset, kKnob, kKnob);
    ShadowPainter::paint(painter, knob, kKnob / 2, s.knobShadow);
    painter.setPen(Qt::NoPen);
    painter.setBrush(s.knob);
    painter.drawEllipse(knob);

    if (hasFocus() && m_keyboardFocus) {
        paintFocusRing(painter, track.adjusted(-3, -3, 3, 3), radius + 3, theme.hoverColor);
    }
}

}  // namespace sky
