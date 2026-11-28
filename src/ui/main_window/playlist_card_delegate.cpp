#include "ui/main_window/playlist_card_delegate.hpp"

#include <QFontMetricsF>
#include <QPainter>

#include "core/translator.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kSideInset = 20;
constexpr qreal kTopInset = 8;
constexpr qreal kCardHeight = 80;
constexpr qreal kRadius = 16;
constexpr qreal kPadding = 16;
constexpr qreal kActionBox = 34;

}  // namespace

PlaylistCardDelegate::PlaylistCardDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QSize PlaylistCardDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return QSize(option.rect.width(), kRowHeight);
}

QRectF PlaylistCardDelegate::cardRect(const QRect &rowRect)
{
    return QRectF(rowRect.left() + kSideInset, rowRect.top() + kTopInset, rowRect.width() - kSideInset * 2, kCardHeight);
}

QRectF PlaylistCardDelegate::actionRect(const QRect &rowRect)
{
    const QRectF card = cardRect(rowRect);
    return QRectF(card.right() - kPadding - kActionBox, card.center().y() - kActionBox / 2, kActionBox, kActionBox);
}

void PlaylistCardDelegate::setHover(int row, bool onAction)
{
    m_hoverRow = row;
    m_hoverAction = onAction;
}

void PlaylistCardDelegate::setOpenMenuRow(int row)
{
    m_openMenuRow = row;
}

void PlaylistCardDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const Theme &theme = currentTheme();
    QPainter &painter = *p;
    painter.save();
    preparePainter(painter);
    const QRectF card = cardRect(option.rect);
    ShadowPainter::paint(painter, card, kRadius, theme.shadowCard);
    paintCardSurface(painter, card, kRadius, theme.cardFill, theme.cardTopEdge, theme.cardTopEdgeHeight);

    const QRectF action = actionRect(option.rect);
    const qreal textLeft = card.left() + kPadding;
    const qreal textWidth = action.left() - 16 - textLeft;
    const QFont nameFont = FontLoader::font(20, QFont::Bold);
    const QFont countFont = FontLoader::font(13, QFont::Normal);
    const QFontMetricsF nameMetrics(nameFont);
    const QFontMetricsF countMetrics(countFont);
    const qreal blockHeight = nameMetrics.height() + countMetrics.height();
    qreal top = card.top() + theme.cardTopEdgeHeight / 2.0 + (card.height() - blockHeight) / 2.0;
    painter.setFont(nameFont);
    painter.setPen(theme.cardText);
    painter.drawText(QRectF(textLeft, top, textWidth, nameMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                     nameMetrics.elidedText(index.data(NameRole).toString(), Qt::ElideRight, textWidth));
    top += nameMetrics.height();
    painter.setFont(countFont);
    painter.setOpacity(0.75);
    painter.drawText(QRectF(textLeft, top, textWidth, countMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                     trCount(index.data(CountRole).toInt(), trNoop("1 sheet"), trNoop("%1 sheets")));
    painter.setOpacity(1.0);

    const bool open = index.row() == m_openMenuRow;
    const bool hovered = index.row() == m_hoverRow && m_hoverAction;
    QColor glyph = theme.actionGlyph;
    if (open) {
        painter.fillPath(roundedPath(action, 8), theme.actionOpenFill);
        glyph = theme.actionOpenGlyph;
    } else if (hovered) {
        painter.fillPath(roundedPath(action, 8), theme.actionHoverFill);
        glyph = theme.actionHoverGlyph;
    }
    const qreal size = open ? 16 : 22;
    painter.drawPixmap(QPointF(action.center().x() - size / 2, action.center().y() - size / 2),
                       IconCache::pixmap(open ? QStringLiteral("x") : QStringLiteral("three-dots-vertical"),
                                         static_cast<int>(size), glyph, painter.device()->devicePixelRatioF()));
    if (option.state & QStyle::State_HasFocus) {
        painter.setPen(QPen(withAlpha(theme.hoverColor, 0.6), 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(card.adjusted(2, 2, -2, -2), kRadius - 2, kRadius - 2);
    }
    painter.restore();
}

}  // namespace sky
