#include "ui/widgets/transport_button.hpp"

#include <QFontMetrics>
#include <QPainter>

#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeight = 32;
constexpr int kMinWidth = 136;
constexpr int kPadding = 9;
constexpr int kIconSize = 16;
constexpr int kIconGap = 5;
constexpr int kBorder = 2;

QFont labelFont()
{
    return FontLoader::font(12, QFont::Bold);
}

}  // namespace

TransportButton::TransportButton(Kind kind, const QString &iconId, QWidget *parent)
    : QAbstractButton(parent), m_kind(kind), m_iconId(iconId), m_hover(new HoverTracker(this, kHoverFastMs))
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedHeight(kHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void TransportButton::setContent(const QString &iconId, const QString &label)
{
    m_iconId = iconId;
    setText(label);
    updateGeometry();
    update();
}

QSize TransportButton::sizeHint() const
{
    const int textWidth = QFontMetrics(labelFont()).horizontalAdvance(text());
    return QSize(std::max(kMinWidth, kPadding * 2 + kBorder * 2 + kIconSize + kIconGap + textWidth), kHeight);
}

void TransportButton::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);

    const QColor idle = m_kind == Kind::Play ? theme.transportPlay : theme.transportSkip;
    const QColor hover = m_kind == Kind::Play ? theme.transportPlayHover : theme.transportSkipHover;
    const QColor color = isDown() ? theme.footerText : blend(idle, hover, m_hover->progress());

    const QRectF box = QRectF(rect()).adjusted(kBorder / 2.0, kBorder / 2.0, -kBorder / 2.0, -kBorder / 2.0);
    const qreal radius = box.height() / 2.0;
    painter.setBrush(theme.transportFill);
    painter.setPen(QPen(color, kBorder));
    painter.drawRoundedRect(box, radius, radius);

    painter.setFont(labelFont());
    const int textWidth = painter.fontMetrics().horizontalAdvance(text());
    const int contentWidth = kIconSize + kIconGap + textWidth;
    const qreal left = (width() - contentWidth) / 2.0;
    painter.drawPixmap(QPointF(left, (height() - kIconSize) / 2.0),
                       IconCache::pixmap(m_iconId, kIconSize, color, devicePixelRatioF()));
    painter.setPen(color);
    painter.drawText(QRectF(left + kIconSize + kIconGap, 0, textWidth + 1, height()), Qt::AlignVCenter | Qt::AlignLeft,
                     text());

    if (hasFocus()) {
        painter.setPen(QPen(withAlpha(color, 0.5), 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box.adjusted(3, 3, -3, -3), radius - 3, radius - 3);
    }
}

}  // namespace sky
