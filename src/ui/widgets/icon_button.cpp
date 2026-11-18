#include "ui/widgets/icon_button.hpp"

#include <QFocusEvent>
#include <QPainter>

#include "ui/core/hover_tracker.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme.hpp"

namespace sky {

IconButton::IconButton(const QString &iconId, int iconSize, QSize boxSize, IconButtonStyle style, QWidget *parent)
    : QAbstractButton(parent),
      m_iconId(iconId),
      m_iconSize(iconSize),
      m_boxSize(boxSize),
      m_style(std::move(style)),
      m_hover(new HoverTracker(this, m_style.durationMs))
{
    setFixedSize(boxSize);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
}

void IconButton::setIconId(const QString &iconId)
{
    m_iconId = iconId;
    update();
}

void IconButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    preparePainter(painter);
    const qreal progress = isDown() ? 1.0 : m_hover->progress();
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    if (m_style.frameFill) {
        const QPainterPath path = roundedPath(box, m_style.frameRadius);
        painter.fillPath(path, m_style.frameFill());
        if (m_style.frameBorder) {
            const QColor border = blend(m_style.frameBorder(), m_style.hover(), progress);
            painter.setPen(QPen(border, 1));
            painter.drawPath(path);
        }
    }

    const QColor color = m_style.hover ? blend(m_style.idle(), m_style.hover(), progress) : m_style.idle();
    const QPixmap glyph = IconCache::pixmap(m_iconId, m_iconSize, isEnabled() ? color : withAlpha(color, 0.45),
                                            devicePixelRatioF());
    painter.translate(rect().center() + QPointF(0.5, 0.5));
    if (m_style.hoverRotation != 0) {
        painter.rotate(m_style.hoverRotation * progress);
    }
    painter.drawPixmap(QPointF(-m_iconSize / 2.0, -m_iconSize / 2.0), glyph);
    if (hasFocus() && m_keyboardFocus) {
        painter.resetTransform();
        painter.setPen(QPen(withAlpha(m_style.hover ? m_style.hover() : color, 0.6), 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box, m_style.frameRadius, m_style.frameRadius);
    }
}

void IconButton::focusInEvent(QFocusEvent *event)
{
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    QAbstractButton::focusInEvent(event);
}

void IconButton::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus = false;
    QAbstractButton::focusOutEvent(event);
}

}  // namespace sky
