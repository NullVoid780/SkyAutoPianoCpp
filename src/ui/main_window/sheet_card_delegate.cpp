#include "ui/main_window/sheet_card_delegate.hpp"

#include <QFontMetricsF>
#include <QPainter>

#include "ui/main_window/library_model.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/icon_cache.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kSideInset = 20;
constexpr qreal kTopInset = 8;
constexpr qreal kCardHeight = 150;
constexpr qreal kRadius = 16;
constexpr qreal kContentInset = 14;
constexpr qreal kPaddingX = 8;
constexpr qreal kPaddingY = 16;
constexpr qreal kTitleGap = 8;
constexpr qreal kRowGap = 6;
constexpr qreal kIconSize = 16;
constexpr qreal kIconGap = 8;
constexpr qreal kActionBox = 34;
constexpr qreal kActionGlyph = 22;
constexpr qreal kActionInset = 10;
constexpr qreal kBadgeHeight = 22;
constexpr qreal kBadgePadding = 8;

QFont titleFont()
{
    QFont font = FontLoader::font(20, QFont::Bold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.2);
    return font;
}

QFont infoFont()
{
    return FontLoader::font(13, QFont::Normal);
}

QFont badgeFont()
{
    return FontLoader::font(12, QFont::Bold);
}

}  // namespace

SheetCardDelegate::SheetCardDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QSize SheetCardDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return QSize(option.rect.width(), kRowHeight);
}

QRectF SheetCardDelegate::cardRect(const QRect &rowRect)
{
    return QRectF(rowRect.left() + kSideInset, rowRect.top() + kTopInset, rowRect.width() - kSideInset * 2, kCardHeight);
}

QRectF SheetCardDelegate::actionRect(const QRect &rowRect)
{
    const QRectF card = cardRect(rowRect);
    return QRectF(card.right() - kActionInset - kActionBox, card.top() + kActionInset, kActionBox, kActionBox);
}

QRectF SheetCardDelegate::checkRect(const QRect &rowRect)
{
    const QRectF card = cardRect(rowRect);
    return QRectF(card.left() + 18, card.center().y() - 11, 22, 22);
}

void SheetCardDelegate::setHover(int row, bool onAction)
{
    m_hoverRow = row;
    m_hoverAction = onAction;
}

void SheetCardDelegate::setOpenMenuRow(int row)
{
    m_openMenuRow = row;
}

void SheetCardDelegate::setCurrentRow(int row)
{
    m_currentRow = row;
}

void SheetCardDelegate::paintInfoRow(QPainter &painter, qreal left, qreal top, qreal width, const QString &icon,
                                     const QColor &tint, const QString &label, const QString &value) const
{
    const Theme &theme = currentTheme();
    const qreal dpr = painter.device()->devicePixelRatioF();
    painter.drawPixmap(QPointF(left, top), IconCache::pixmap(icon, static_cast<int>(kIconSize), tint, dpr));
    painter.setFont(infoFont());
    const QFontMetricsF metrics(infoFont());
    qreal x = left + kIconSize + kIconGap;
    const QRectF line(x, top, width - (x - left), kIconSize);
    painter.setPen(theme.cardLabelTint);
    painter.drawText(line, Qt::AlignVCenter | Qt::AlignLeft, label);
    x += metrics.horizontalAdvance(label + QLatin1Char(' '));
    painter.setPen(theme.cardText);
    const QString elided = metrics.elidedText(value, Qt::ElideRight, std::max<qreal>(0, left + width - x));
    painter.drawText(QRectF(x, top, left + width - x, kIconSize), Qt::AlignVCenter | Qt::AlignLeft, elided);
}

void SheetCardDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const Theme &theme = currentTheme();
    QPainter &painter = *p;
    painter.save();
    preparePainter(painter);

    const QRectF card = cardRect(option.rect);
    const bool bulk = index.data(LibraryModel::BulkModeRole).toBool();
    const bool selected = bulk && index.data(LibraryModel::SelectedRole).toBool();
    ShadowPainter::paint(painter, card, kRadius, theme.shadowCard);
    const QColor fill = selected ? blend(theme.cardFill, theme.cardSelectedTint, 0.12) : theme.cardFill;
    paintCardSurface(painter, card, kRadius, fill, theme.cardTopEdge, theme.cardTopEdgeHeight);
    if (selected) {
        painter.setPen(QPen(theme.cardSelectedOutline, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(card.adjusted(1, 1, -1, -1), kRadius - 1, kRadius - 1);
    }
    if (bulk) {
        const QRectF check = checkRect(option.rect);
        const QColor tint = selected ? theme.cardSelectedTint : theme.cardText;
        const QString glyph = selected ? QStringLiteral("check-square-ticked") : QStringLiteral("check-square");
        painter.drawPixmap(QPointF(check.center().x() - 9, check.center().y() - 9),
                           IconCache::pixmap(glyph, 18, tint, painter.device()->devicePixelRatioF()));
    }

    const qreal left = card.left() + (bulk ? 58 : kContentInset) + kPaddingX;
    const qreal right = bulk ? card.right() - kPaddingX : card.right() - kActionInset - kActionBox - kPaddingX;
    qreal top = card.top() + theme.cardTopEdgeHeight + kPaddingY;

    const QString name = index.data(LibraryModel::NameRole).toString();
    const bool hold = index.data(LibraryModel::HoldRole).toBool();
    painter.setFont(titleFont());
    const QFontMetricsF titleMetrics(titleFont());
    const QString badgeText = QStringLiteral("Hold");
    const qreal badgeWidth = QFontMetricsF(badgeFont()).horizontalAdvance(badgeText) + kBadgePadding * 2;
    const qreal titleSpace = right - left - (hold ? badgeWidth + 8 : 0);
    const QString title = titleMetrics.elidedText(name, Qt::ElideRight, std::max<qreal>(0, titleSpace));
    const qreal titleHeight = std::max<qreal>(titleMetrics.height(), hold ? kBadgeHeight : 0);
    painter.setPen(theme.cardText);
    painter.drawText(QRectF(left, top, titleSpace, titleHeight), Qt::AlignVCenter | Qt::AlignLeft, title);
    if (hold) {
        const QRectF badge(left + titleMetrics.horizontalAdvance(title) + 8, top + (titleHeight - kBadgeHeight) / 2,
                           badgeWidth, kBadgeHeight);
        paintPill(painter, badge, theme.holdBadgeFill);
        painter.setFont(badgeFont());
        painter.setPen(theme.cardText);
        painter.drawText(badge, Qt::AlignCenter, badgeText);
    }
    top += titleHeight + kTitleGap;

    painter.setOpacity(0.95);
    const qreal infoWidth = card.right() - kPaddingX - left;
    paintInfoRow(painter, left, top, infoWidth, QStringLiteral("person"), theme.authorIcon, QStringLiteral("Author:"),
                 index.data(LibraryModel::AuthorRole).toString());
    top += kIconSize + kRowGap;
    paintInfoRow(painter, left, top, infoWidth, QStringLiteral("edit-note"), theme.transcriberIcon,
                 QStringLiteral("Transcript by:"), index.data(LibraryModel::TranscriberRole).toString());
    top += kIconSize + kRowGap;
    paintInfoRow(painter, left, top, infoWidth, QStringLiteral("metronome"), theme.bpmIcon, QStringLiteral("BPM:"),
                 index.data(LibraryModel::BpmRole).toString());
    painter.setOpacity(1.0);

    if (bulk) {
        painter.restore();
        return;
    }
    const QRectF action = actionRect(option.rect);
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
    const qreal glyphSize = open ? 16 : kActionGlyph;
    painter.drawPixmap(QPointF(action.center().x() - glyphSize / 2, action.center().y() - glyphSize / 2),
                       IconCache::pixmap(open ? QStringLiteral("x") : QStringLiteral("three-dots-vertical"),
                                         static_cast<int>(glyphSize), glyph, painter.device()->devicePixelRatioF()));

    if (option.state & QStyle::State_HasFocus) {
        painter.setPen(QPen(withAlpha(theme.hoverColor, 0.6), 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(card.adjusted(2, 2, -2, -2), kRadius - 2, kRadius - 2);
    }
    painter.restore();
}

}  // namespace sky
