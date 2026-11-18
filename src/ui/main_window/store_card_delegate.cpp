#include "ui/main_window/store_card_delegate.hpp"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>

#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kSideInset = 20;
constexpr qreal kTopInset = 8;
constexpr qreal kBottomGap = 2;
constexpr qreal kRadius = 16;
constexpr qreal kPadY = 18;
constexpr qreal kPadX = 16;
constexpr qreal kColumnGap = 16;
constexpr qreal kItemGap = 10;
constexpr qreal kBadgeGap = 8;
constexpr qreal kBadgePadY = 4;
constexpr qreal kBadgePadX = 8;
constexpr qreal kButtonHeight = 30;
constexpr qreal kButtonPadX = 10;
constexpr qreal kButtonIcon = 14;
constexpr qreal kButtonIconGap = 5;
constexpr qreal kButtonGap = 8;
constexpr qreal kButtonRadius = 8;

QFont titleFont()
{
    return FontLoader::font(20, QFont::Bold);
}

QFont bodyFont()
{
    return FontLoader::font(13, QFont::Normal);
}

QFont badgeFont()
{
    return FontLoader::font(12, QFont::DemiBold);
}

QFont buttonFont(bool play)
{
    return FontLoader::font(12, play ? QFont::DemiBold : QFont::Normal);
}

qreal buttonWidth(const QString &label, bool play)
{
    return kButtonPadX * 2 + kButtonIcon + kButtonIconGap + QFontMetricsF(buttonFont(play)).horizontalAdvance(label) + 2;
}

qreal wrappedHeight(const QFont &font, qreal width, const QString &text)
{
    return QFontMetricsF(font).boundingRect(QRectF(0, 0, width, 1e6), Qt::TextWordWrap, text).height();
}

}  // namespace

StoreCardDelegate::StoreCardDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void StoreCardDelegate::setViewWidth(int width)
{
    m_viewWidth = width;
}

void StoreCardDelegate::setHover(int row, Button button)
{
    m_hoverRow = row;
    m_hoverButton = button;
}

void StoreCardDelegate::setPressed(int row, Button button)
{
    m_pressedRow = row;
    m_pressedButton = button;
}

StoreCardDelegate::Layout StoreCardDelegate::layoutFor(const QRect &rowRect, const QModelIndex &index) const
{
    const Theme &theme = currentTheme();
    Layout layout;
    const qreal width = (rowRect.width() > 0 ? rowRect.width() : m_viewWidth) - kSideInset * 2;
    const QString playLabel = index.data(PlayLabelRole).toString();
    const QString saveLabel = index.data(SaveLabelRole).toString();
    const qreal actionsWidth = std::max(buttonWidth(playLabel, true), buttonWidth(saveLabel, false));
    const qreal left = rowRect.left() + kSideInset + kPadX;
    const qreal mainWidth = std::max<qreal>(80, width - kPadX * 2 - actionsWidth - kColumnGap);
    qreal y = rowRect.top() + kTopInset + theme.cardTopEdgeHeight + kPadY;

    const qreal titleHeight = wrappedHeight(titleFont(), mainWidth, index.data(TitleRole).toString());
    layout.title = QRectF(left, y, mainWidth, titleHeight);
    y += titleHeight + kItemGap;
    const qreal metaHeight = wrappedHeight(bodyFont(), mainWidth, index.data(MetaRole).toString());
    layout.meta = QRectF(left, y, mainWidth, metaHeight);
    y += metaHeight + kItemGap;

    const QFontMetricsF badgeMetrics(badgeFont());
    const qreal badgeHeight = badgeMetrics.height() + kBadgePadY * 2;
    qreal x = left;
    for (const QString &badge : index.data(BadgesRole).toStringList()) {
        const qreal w = badgeMetrics.horizontalAdvance(badge) + kBadgePadX * 2;
        if (x > left && x + w > left + mainWidth) {
            x = left;
            y += badgeHeight + kBadgeGap;
        }
        layout.badges.emplace_back(x, y, w, badgeHeight);
        x += w + kBadgeGap;
    }
    y += badgeHeight;
    const QString status = index.data(StatusRole).toString();
    if (!status.isEmpty()) {
        y += kItemGap;
        const qreal statusHeight = wrappedHeight(bodyFont(), mainWidth, status);
        layout.status = QRectF(left, y, mainWidth, statusHeight);
        y += statusHeight;
    }

    const qreal actionsLeft = rowRect.left() + kSideInset + width - kPadX - actionsWidth;
    const qreal actionsTop = rowRect.top() + kTopInset + theme.cardTopEdgeHeight + kPadY;
    layout.play = QRectF(actionsLeft, actionsTop, actionsWidth, kButtonHeight);
    layout.save = QRectF(actionsLeft, actionsTop + kButtonHeight + kButtonGap, actionsWidth, kButtonHeight);

    const qreal bottom = std::max(y, layout.save.bottom()) + kPadY;
    layout.card = QRectF(rowRect.left() + kSideInset, rowRect.top() + kTopInset, width, bottom - (rowRect.top() + kTopInset));
    return layout;
}

QSize StoreCardDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QRect probe(0, 0, option.rect.width() > 0 ? option.rect.width() : m_viewWidth, 0);
    const Layout layout = layoutFor(probe, index);
    return QSize(probe.width(), qCeil(layout.card.bottom() + kBottomGap));
}

StoreCardDelegate::Button StoreCardDelegate::buttonAt(const QRect &rowRect, const QModelIndex &index, const QPointF &point) const
{
    const Layout layout = layoutFor(rowRect, index);
    if (layout.play.contains(point)) {
        return Button::Play;
    }
    if (layout.save.contains(point)) {
        return Button::Save;
    }
    return Button::None;
}

void StoreCardDelegate::paintButton(QPainter &painter, const QRectF &rect, const QString &icon, const QString &label,
                                    bool play, bool enabled, bool hovered, bool pressed) const
{
    const Theme &theme = currentTheme();
    const QPainterPath path = roundedPath(rect.adjusted(0.5, 0.5, -0.5, -0.5), kButtonRadius);
    QColor border;
    QColor text;
    QColor fill = theme.accent.transparent;
    if (play) {
        border = theme.storePlay;
        text = theme.storePlay;
        if (!enabled) {
            border = theme.storePlayDisabledBorder;
            text = theme.storePlayDisabledText;
        } else if (pressed) {
            border = text = theme.storePlayPressed;
            fill = theme.storePlayPressedFill;
        } else if (hovered) {
            border = text = theme.storePlayHover;
            fill = theme.storePlayHoverFill;
        }
    } else {
        fill = enabled ? theme.cardFill : theme.neutralFillDisabled;
        border = enabled ? (hovered ? theme.hoverColor : theme.neutralBorder) : theme.neutralBorderDisabled;
        text = enabled && hovered ? theme.hoverColor : theme.cardText;
    }
    painter.save();
    if (!enabled) {
        painter.setOpacity(play ? 0.5 : 0.72);
    }
    painter.fillPath(path, fill);
    painter.setPen(QPen(border, 1));
    painter.drawPath(path);
    const QFont font = buttonFont(play);
    const qreal textWidth = QFontMetricsF(font).horizontalAdvance(label);
    const qreal contentWidth = kButtonIcon + kButtonIconGap + textWidth;
    const qreal x = rect.center().x() - contentWidth / 2;
    painter.drawPixmap(QPointF(x, rect.center().y() - kButtonIcon / 2),
                       IconCache::pixmap(icon, static_cast<int>(kButtonIcon), text, painter.device()->devicePixelRatioF()));
    painter.setFont(font);
    painter.setPen(text);
    painter.drawText(QRectF(x + kButtonIcon + kButtonIconGap, rect.top(), textWidth + 1, rect.height()),
                     Qt::AlignVCenter | Qt::AlignLeft, label);
    painter.restore();
}

void StoreCardDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const Theme &theme = currentTheme();
    QPainter &painter = *p;
    painter.save();
    preparePainter(painter);
    const Layout layout = layoutFor(option.rect, index);
    ShadowPainter::paint(painter, layout.card, kRadius, theme.shadowCard);
    paintCardSurface(painter, layout.card, kRadius, theme.cardFill, theme.cardTopEdge, theme.cardTopEdgeHeight);

    painter.setPen(theme.cardText);
    painter.setFont(titleFont());
    painter.drawText(layout.title, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, index.data(TitleRole).toString());
    painter.setFont(bodyFont());
    painter.setOpacity(0.8);
    painter.drawText(layout.meta, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, index.data(MetaRole).toString());
    if (!layout.status.isEmpty()) {
        painter.drawText(layout.status, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, index.data(StatusRole).toString());
    }
    painter.setOpacity(1.0);

    painter.setFont(badgeFont());
    const QStringList badges = index.data(BadgesRole).toStringList();
    for (std::size_t i = 0; i < layout.badges.size() && i < static_cast<std::size_t>(badges.size()); ++i) {
        paintPill(painter, layout.badges[i], theme.storeBadgeFill);
        painter.setPen(theme.storeBadgeText);
        painter.drawText(layout.badges[i], Qt::AlignCenter, badges[static_cast<qsizetype>(i)]);
    }

    const int row = index.row();
    paintButton(painter, layout.play, QStringLiteral("play-fill"), index.data(PlayLabelRole).toString(), true,
                index.data(CanPlayRole).toBool(), row == m_hoverRow && m_hoverButton == Button::Play,
                row == m_pressedRow && m_pressedButton == Button::Play);
    paintButton(painter, layout.save, QStringLiteral("music-note-list"), index.data(SaveLabelRole).toString(), false,
                index.data(CanSaveRole).toBool(), row == m_hoverRow && m_hoverButton == Button::Save,
                row == m_pressedRow && m_pressedButton == Button::Save);
    painter.restore();
}

}  // namespace sky
