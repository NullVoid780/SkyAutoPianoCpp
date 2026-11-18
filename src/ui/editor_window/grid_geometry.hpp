#pragma once

#include <QRectF>

namespace sky {

class GridGeometry {
public:
    static constexpr int kColumns = 4;
    static constexpr qreal kGap = 15;
    static constexpr qreal kPadding = 5;
    static constexpr qreal kCaptionGap = 5;
    static constexpr qreal kCaptionHeight = 16;
    static constexpr qreal kCellPadding = 10;
    static constexpr qreal kDotGap = 3;

    explicit GridGeometry(qreal width = 0, int cellCount = 0);

    qreal cellWidth() const { return m_cellWidth; }
    qreal cellHeight() const { return m_cellWidth * 9.0 / 16.0; }
    qreal rowHeight() const { return cellHeight() + kCaptionGap + kCaptionHeight; }
    qreal contentHeight() const;
    int rowCount() const;

    QRectF cellRect(int index) const;
    QRectF captionRect(int index) const;
    QRectF dotSlot(const QRectF &cell, int dot) const;
    int cellAt(const QPointF &point) const;
    int firstRowIn(qreal top) const;
    int lastRowIn(qreal bottom) const;

private:
    qreal m_width;
    int m_cellCount;
    qreal m_cellWidth;
};

}  // namespace sky
