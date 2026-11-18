#include "ui/main_window/nav_tab.hpp"

#include <QFontMetrics>
#include <QPainter>

#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kMinHeight = 42;
constexpr int kPadX = 12;
constexpr int kIcon = 16;
constexpr int kGap = 8;
constexpr qreal kRadius = 8;

QFont tabFont()
{
    return FontLoader::font(14, QFont::Medium);
}

}  // namespace

NavTab::NavTab(const QString &iconId, const QString &label, QWidget *parent)
    : QAbstractButton(parent), m_iconId(iconId), m_hover(new HoverTracker(this, kHoverSlowMs))
{
    setText(label);
    setCheckable(true);
    setAutoExclusive(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(kMinHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize NavTab::sizeHint() const
{
    return QSize(kPadX * 2 + kIcon + kGap + QFontMetrics(tabFont()).horizontalAdvance(text()), kMinHeight);
}

void NavTab::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const qreal hover = m_hover->progress();
    const bool active = isChecked();
    const qreal lift = theme.kind == AppTheme::Dark && (active || hover > 0) ? (active ? 1.0 : hover) : (active ? 1.0 : 0.0);
    const QRectF box = QRectF(rect()).translated(0, -lift).adjusted(0, 0, 0, 0);
    const QPainterPath path = roundedPath(box, kRadius);

    QColor ink;
    if (active) {
        painter.fillPath(path, theme.navActiveStart);
        ink = theme.accent.white;
    } else {
        painter.fillPath(path, withAlpha(theme.navHoverFill, hover));
        ink = blend(theme.navIdleText, theme.navHoverText, hover);
    }

    const qreal iconTop = box.center().y() - kIcon / 2.0;
    painter.drawPixmap(QPointF(box.left() + kPadX, iconTop), IconCache::pixmap(m_iconId, kIcon, ink, devicePixelRatioF()));
    painter.setFont(tabFont());
    painter.setPen(ink);
    painter.drawText(box.adjusted(kPadX + kIcon + kGap, 0, -kPadX, 0), Qt::AlignVCenter | Qt::AlignLeft, this->text());
    if (hasFocus()) {
        painter.setPen(QPen(withAlpha(theme.hoverColor, 0.6), 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(box.adjusted(1.5, 1.5, -1.5, -1.5), kRadius - 1, kRadius - 1);
    }
}

}  // namespace sky
