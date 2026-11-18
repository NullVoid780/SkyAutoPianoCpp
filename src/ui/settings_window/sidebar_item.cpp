#include "ui/settings_window/sidebar_item.hpp"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QPainter>

#include "ui/core/hover_tracker.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kHeight = 48;
constexpr int kPadX = 14;
constexpr int kIconSize = 20;
constexpr int kGap = 14;
constexpr int kBadge = 8;

}  // namespace

SidebarItem::SidebarItem(const QString &iconId, const QString &label, QWidget *parent)
    : QAbstractButton(parent), m_iconId(iconId), m_hover(new HoverTracker(this, kHoverMs, Easing::Ease))
{
    setText(label);
    setCheckable(true);
    setAutoExclusive(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setFixedHeight(kHeight);
    connect(m_hover, &HoverTracker::changed, this, [this] { update(); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

QSize SidebarItem::sizeHint() const
{
    return QSize(200, kHeight);
}

void SidebarItem::setBadge(bool visible)
{
    if (visible != m_badge) {
        m_badge = visible;
        update();
    }
}

void SidebarItem::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        click();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void SidebarItem::focusInEvent(QFocusEvent *event)
{
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    QAbstractButton::focusInEvent(event);
}

void SidebarItem::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus = false;
    QAbstractButton::focusOutEvent(event);
}

void SidebarItem::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    const SettingsPalette &s = theme.settings;
    QPainter painter(this);
    preparePainter(painter);
    const qreal hover = m_hover->progress();
    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath path = roundedPath(body, kRadius);

    QColor ink = blend(s.cardDescText, theme.fontColor, hover);
    if (isChecked()) {
        painter.fillPath(path, s.subtleHoverFill);
        ink = theme.fontColor;
    } else if (hover > 0) {
        painter.fillPath(path, withAlpha(s.subtleFill, hover));
    }

    const qreal iconLeft = body.left() + kPadX;
    painter.drawPixmap(QPointF(iconLeft, body.center().y() - kIconSize / 2.0),
                       IconCache::pixmap(m_iconId, kIconSize, ink, devicePixelRatioF()));
    const qreal textLeft = iconLeft + kIconSize + kGap;
    const qreal textRight = body.right() - kPadX - (m_badge ? kBadge + kGap : 0);
    painter.setFont(FontLoader::font(15, isChecked() ? QFont::DemiBold : QFont::Medium));
    painter.setPen(ink);
    painter.drawText(QRectF(textLeft, body.top(), textRight - textLeft, body.height()), Qt::AlignVCenter | Qt::AlignLeft,
                     text());

    if (m_badge) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme.accent.blue500);
        painter.drawEllipse(QRectF(body.right() - kPadX - kBadge, body.center().y() - kBadge / 2.0, kBadge, kBadge));
    }
    if (hasFocus() && m_keyboardFocus) {
        paintFocusRing(painter, body.adjusted(1.5, 1.5, -1.5, -1.5), kRadius - 1, theme.hoverColor);
    }
}

}  // namespace sky
