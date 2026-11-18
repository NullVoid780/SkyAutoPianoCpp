#include "ui/widgets/state_card.hpp"

#include <QFontMetricsF>
#include <QPainter>

#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kPadY = 18;
constexpr qreal kPadX = 16;
constexpr qreal kGap = 12;
constexpr qreal kRadius = 16;
constexpr int kShadowRoom = 18;

QFont titleFont()
{
    return FontLoader::font(18, QFont::Bold);
}

QFont bodyFont()
{
    return FontLoader::font(13, QFont::Normal);
}

}  // namespace

StateCard::StateCard(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void StateCard::setContent(const QString &title, const QString &body)
{
    m_title = title;
    m_body = body;
    updateGeometry();
    update();
}

int StateCard::heightForWidth(int width) const
{
    const qreal inner = width - kShadowRoom * 2 - kPadX * 2;
    qreal height = kPadY * 2 + currentTheme().cardTopEdgeHeight;
    height += QFontMetricsF(titleFont()).boundingRect(QRectF(0, 0, inner, 1e6), Qt::TextWordWrap, m_title).height();
    if (!m_body.isEmpty()) {
        height += kGap + QFontMetricsF(bodyFont()).boundingRect(QRectF(0, 0, inner, 1e6), Qt::TextWordWrap, m_body).height();
    }
    return qCeil(height) + kShadowRoom * 2;
}

QSize StateCard::sizeHint() const
{
    return QSize(400, heightForWidth(400));
}

void StateCard::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    const QRectF card = QRectF(rect()).adjusted(kShadowRoom, kShadowRoom, -kShadowRoom, -kShadowRoom);
    ShadowPainter::paint(painter, card, kRadius, theme.shadowCard);
    const int edge = theme.kind == AppTheme::Light ? theme.cardTopEdgeHeight : 0;
    paintCardSurface(painter, card, kRadius, theme.cardFill, edge > 0 ? theme.cardTopEdge : theme.cardFill, edge);

    const QRectF inner = card.adjusted(kPadX, kPadY + edge, -kPadX, -kPadY);
    painter.setPen(theme.cardText);
    painter.setFont(titleFont());
    const QRectF titleBounds = painter.boundingRect(inner, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, m_title);
    painter.drawText(inner, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, m_title);
    if (!m_body.isEmpty()) {
        painter.setFont(bodyFont());
        painter.setOpacity(0.8);
        painter.drawText(inner.adjusted(0, titleBounds.height() + kGap, 0, 0), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                         m_body);
    }
}

}  // namespace sky
