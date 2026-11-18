#include "ui/render/paint_utils.hpp"

namespace sky {

void preparePainter(QPainter &painter)
{
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
}

QPainterPath roundedPath(const QRectF &rect, qreal radius)
{
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    return path;
}

void paintCardSurface(QPainter &painter, const QRectF &rect, qreal radius, const QColor &fill, const QColor &topEdge,
                      int topEdgeHeight)
{
    const QPainterPath path = roundedPath(rect, radius);
    painter.fillPath(path, topEdge);
    painter.save();
    painter.setClipPath(path);
    painter.fillRect(rect.adjusted(0, topEdgeHeight, 0, 0), fill);
    painter.restore();
}

void paintPill(QPainter &painter, const QRectF &rect, const QColor &fill)
{
    painter.fillPath(roundedPath(rect, rect.height() / 2.0), fill);
}

void paintFocusRing(QPainter &painter, const QRectF &rect, qreal radius, const QColor &color)
{
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, 1, Qt::DotLine));
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    painter.restore();
}

}  // namespace sky
