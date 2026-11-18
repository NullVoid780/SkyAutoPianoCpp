#include "ui/main_window/sheet_actions_menu.hpp"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kMargin = 60;
constexpr qreal kPadding = 6;
constexpr qreal kGap = 2;
constexpr qreal kMinWidth = 180;
constexpr qreal kRadius = 14;
constexpr qreal kEntryRadius = 10;
constexpr qreal kEntryPadX = 10;
constexpr qreal kEntryPadY = 9;
constexpr qreal kIcon = 18;
constexpr qreal kIconGap = 10;
constexpr int kPlacementGap = 8;
constexpr int kWindowInset = 8;

QFont entryFont()
{
    return FontLoader::font(13, QFont::Normal);
}

qreal entryHeight()
{
    return std::max(kIcon, QFontMetricsF(entryFont()).height()) + kEntryPadY * 2;
}

}  // namespace

SheetActionsMenu::SheetActionsMenu(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void SheetActionsMenu::setEntries(std::vector<SheetActionEntry> entries)
{
    m_entries = std::move(entries);
    m_hovered = -1;
    resize(panelSize() + QSize(kMargin * 2, kMargin * 2));
    update();
}

QSize SheetActionsMenu::panelSize() const
{
    const QFontMetricsF metrics(entryFont());
    qreal width = kMinWidth;
    for (const SheetActionEntry &entry : m_entries) {
        width = std::max(width, kPadding * 2 + kEntryPadX * 2 + kIcon + kIconGap + metrics.horizontalAdvance(entry.label) + 2);
    }
    const auto count = static_cast<qreal>(m_entries.size());
    const qreal height = kPadding * 2 + count * entryHeight() + std::max<qreal>(0, count - 1) * kGap;
    return QSizeF(width, height).toSize();
}

QRectF SheetActionsMenu::panelRect() const
{
    return QRectF(QPointF(kMargin, kMargin), QSizeF(panelSize()));
}

QRectF SheetActionsMenu::entryRect(int index) const
{
    const QRectF panel = panelRect();
    return QRectF(panel.left() + kPadding, panel.top() + kPadding + index * (entryHeight() + kGap),
                  panel.width() - kPadding * 2, entryHeight());
}

int SheetActionsMenu::entryAt(const QPointF &point) const
{
    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
        if (entryRect(i).contains(point)) {
            return i;
        }
    }
    return -1;
}

void SheetActionsMenu::popupBeside(const QRect &anchor, const QRect &window)
{
    const QSize panel = panelSize();
    const int spaceBelow = window.bottom() - anchor.bottom();
    const int spaceAbove = anchor.top() - window.top();
    int top = anchor.bottom() + kPlacementGap;
    if (spaceBelow < panel.height() + kPlacementGap && spaceAbove > spaceBelow) {
        top = anchor.top() - kPlacementGap - panel.height();
    }
    int left = anchor.right() - panel.width();
    left = std::clamp(left, window.left() + kWindowInset, std::max(window.left() + kWindowInset, window.right() - kWindowInset - panel.width()));
    move(left - kMargin, top - kMargin);
    show();
    setFocus();
}

void SheetActionsMenu::activate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_entries.size()) || !m_entries[static_cast<std::size_t>(index)].enabled) {
        return;
    }
    const SheetAction action = m_entries[static_cast<std::size_t>(index)].action;
    hide();
    emit triggered(action);
}

void SheetActionsMenu::mouseMoveEvent(QMouseEvent *event)
{
    const int hovered = entryAt(event->position());
    if (hovered != m_hovered) {
        m_hovered = hovered;
        setCursor(hovered >= 0 && m_entries[static_cast<std::size_t>(hovered)].enabled ? Qt::PointingHandCursor
                                                                                       : Qt::ArrowCursor);
        update();
    }
}

void SheetActionsMenu::leaveEvent(QEvent *)
{
    m_hovered = -1;
    update();
}

void SheetActionsMenu::mouseReleaseEvent(QMouseEvent *event)
{
    if (!panelRect().contains(event->position())) {
        hide();
        return;
    }
    activate(entryAt(event->position()));
}

void SheetActionsMenu::keyPressEvent(QKeyEvent *event)
{
    const int count = static_cast<int>(m_entries.size());
    switch (event->key()) {
    case Qt::Key_Escape:
        hide();
        return;
    case Qt::Key_Down:
        m_hovered = count ? (m_hovered + 1 + count) % count : -1;
        update();
        return;
    case Qt::Key_Up:
        m_hovered = count ? (m_hovered - 1 + count) % count : -1;
        update();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        activate(m_hovered);
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

void SheetActionsMenu::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit closed();
}

void SheetActionsMenu::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF panel = panelRect();
    ShadowPainter::paint(painter, panel, kRadius, theme.shadowPopover);
    const QPainterPath path = roundedPath(panel.adjusted(0.5, 0.5, -0.5, -0.5), kRadius);
    painter.fillPath(path, theme.popoverFill);
    painter.setPen(QPen(theme.popoverBorder, 1));
    painter.drawPath(path);

    painter.setFont(entryFont());
    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
        const SheetActionEntry &entry = m_entries[static_cast<std::size_t>(i)];
        const QRectF rect = entryRect(i);
        const bool hovered = i == m_hovered && entry.enabled;
        QColor text = entry.destructive ? theme.dangerText : theme.fontColor;
        if (hovered) {
            painter.fillPath(roundedPath(rect, kEntryRadius), entry.destructive ? theme.dangerHoverFill : theme.menuHoverFill);
            text = entry.destructive ? theme.dangerHoverText : theme.menuHoverText;
        }
        QColor iconColor = text;
        if (entry.action == SheetAction::Favorite) {
            iconColor = entry.favorite ? theme.heartOn : theme.heartOff;
        } else if (entry.action == SheetAction::Edit) {
            iconColor = theme.editIcon;
        } else if (entry.action == SheetAction::Delete) {
            iconColor = theme.trashIcon;
        }
        painter.setOpacity(entry.enabled ? 1.0 : 0.42);
        painter.drawPixmap(QPointF(rect.left() + kEntryPadX, rect.center().y() - kIcon / 2),
                           IconCache::pixmap(entry.icon, static_cast<int>(kIcon), iconColor, devicePixelRatioF()));
        painter.setPen(text);
        painter.drawText(rect.adjusted(kEntryPadX + kIcon + kIconGap, 0, -kEntryPadX, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         entry.label);
        painter.setOpacity(1.0);
    }
}

}  // namespace sky
