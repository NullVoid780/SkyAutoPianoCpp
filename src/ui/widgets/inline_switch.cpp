#include "ui/widgets/inline_switch.hpp"

#include <QFontMetricsF>
#include <QPainter>
#include <QSignalBlocker>
#include <QVariantAnimation>

#include "ui/core/easing.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kTrackWidth = 25;
constexpr qreal kTrackHeight = 15;
constexpr qreal kKnob = 13;
constexpr qreal kKnobTravel = 10;
constexpr qreal kTrackGap = 8;
constexpr qreal kStateWidth = 25;
constexpr int kHeight = 22;

QFont switchFont(bool bold)
{
    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(12);
    font.setBold(bold);
    return font;
}

qreal textGap()
{
    return 0.3 * 12;
}

}  // namespace

InlineSwitch::InlineSwitch(const QString &label, QWidget *parent)
    : QAbstractButton(parent), m_animation(new QVariantAnimation(this))
{
    setText(label);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    m_animation->setDuration(kLongPressKnobMs);
    m_animation->setEasingCurve(easingCurve(Easing::EaseOut));
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_position = value.toReal();
        update();
    });
}

QSize InlineSwitch::sizeHint() const
{
    const qreal textWidth = QFontMetricsF(switchFont(false)).horizontalAdvance(text());
    return QSize(qCeil(kTrackWidth + kTrackGap + textWidth + textGap() + kStateWidth), kHeight);
}

void InlineSwitch::setCheckedSilently(bool checked)
{
    const QSignalBlocker blocker(this);
    setChecked(checked);
    m_animation->stop();
    m_position = checked ? 1.0 : 0.0;
    update();
}

void InlineSwitch::checkStateSet()
{
    animate();
}

void InlineSwitch::nextCheckState()
{
    QAbstractButton::nextCheckState();
    animate();
}

void InlineSwitch::animate()
{
    m_animation->stop();
    m_animation->setStartValue(m_position);
    m_animation->setEndValue(isChecked() ? 1.0 : 0.0);
    m_animation->start();
}

void InlineSwitch::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);

    const qreal top = (height() - kTrackHeight) / 2.0;
    const QRectF track(0, top, kTrackWidth, kTrackHeight);
    paintPill(painter, track, blend(theme.switchTrackOff, theme.hoverColor, m_position));

    const QRectF knob(1 + kKnobTravel * m_position, top + 1, kKnob, kKnob);
    for (const ShadowSpec &shadow : {ShadowSpec{1, {0, 3}, withAlpha(theme.switchKnobShadow, 0.5)},
                                     ShadowSpec{2, {0, 2}, theme.switchKnobShadow},
                                     ShadowSpec{3, {0, 3}, withAlpha(theme.switchKnobShadow, 0.5)}}) {
        ShadowPainter::paint(painter, knob, kKnob / 2.0, shadow);
    }
    paintPill(painter, knob, theme.switchKnob);

    painter.setFont(switchFont(false));
    painter.setPen(theme.footerText);
    const qreal textLeft = kTrackWidth + kTrackGap;
    const qreal textWidth = QFontMetricsF(painter.font()).horizontalAdvance(text());
    const QRectF textRect(textLeft, top, textWidth + 1, kTrackHeight);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());

    const QRectF stateRect(textLeft + textWidth + textGap(), top, kStateWidth, kTrackHeight);
    painter.save();
    painter.setClipRect(stateRect);
    painter.setFont(switchFont(true));
    QColor offColor = theme.footerText;
    offColor.setAlphaF(1.0 - m_position);
    painter.setPen(offColor);
    painter.drawText(stateRect.translated(0, -kTrackHeight * m_position), Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("Off"));
    QColor onColor = theme.hoverColor;
    onColor.setAlphaF(m_position);
    painter.setPen(onColor);
    painter.drawText(stateRect.translated(0, kTrackHeight * (1.0 - m_position)), Qt::AlignVCenter | Qt::AlignLeft,
                     QStringLiteral("On"));
    painter.restore();

    if (hasFocus()) {
        painter.setPen(QPen(withAlpha(theme.hoverColor, 0.6), 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(-2, -2, 2, 2), 9, 9);
    }
}

}  // namespace sky
