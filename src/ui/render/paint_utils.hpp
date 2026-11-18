#pragma once

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>

namespace sky {

void preparePainter(QPainter &painter);
QPainterPath roundedPath(const QRectF &rect, qreal radius);
void paintCardSurface(QPainter &painter, const QRectF &rect, qreal radius, const QColor &fill, const QColor &topEdge,
                      int topEdgeHeight);
void paintPill(QPainter &painter, const QRectF &rect, const QColor &fill);
void paintFocusRing(QPainter &painter, const QRectF &rect, qreal radius, const QColor &color);

}  // namespace sky
