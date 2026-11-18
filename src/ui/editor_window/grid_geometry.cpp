#include "ui/editor_window/grid_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace sky {

namespace {

constexpr int kDotColumns = 5;
constexpr int kDotRows = 3;

}  // namespace

GridGeometry::GridGeometry(qreal width, int cellCount)
    : m_width(width),
      m_cellCount(cellCount),
      m_cellWidth(std::max<qreal>(0, (width - kPadding * 2 - kGap * (kColumns - 1)) / kColumns))
{
}

int GridGeometry::rowCount() const
{
    return (m_cellCount + kColumns - 1) / kColumns;
}

qreal GridGeometry::contentHeight() const
{
    const int rows = rowCount();
    return rows == 0 ? kPadding * 2 : kPadding * 2 + rows * rowHeight() + (rows - 1) * kGap;
}

QRectF GridGeometry::cellRect(int index) const
{
    const int row = index / kColumns;
    const int column = index % kColumns;
    return QRectF(kPadding + column * (m_cellWidth + kGap), kPadding + row * (rowHeight() + kGap), m_cellWidth,
                  cellHeight());
}

QRectF GridGeometry::captionRect(int index) const
{
    const QRectF cell = cellRect(index);
    return QRectF(cell.left(), cell.bottom() + kCaptionGap, cell.width(), kCaptionHeight);
}

QRectF GridGeometry::dotSlot(const QRectF &cell, int dot) const
{
    const QRectF inner = cell.adjusted(kCellPadding, kCellPadding, -kCellPadding, -kCellPadding);
    const qreal slotWidth = (inner.width() - kDotGap * (kDotColumns - 1)) / kDotColumns;
    const qreal slotHeight = (inner.height() - kDotGap * (kDotRows - 1)) / kDotRows;
    const int column = dot % kDotColumns;
    const int row = dot / kDotColumns;
    return QRectF(inner.left() + column * (slotWidth + kDotGap), inner.top() + row * (slotHeight + kDotGap), slotWidth,
                  slotHeight);
}

int GridGeometry::cellAt(const QPointF &point) const
{
    if (m_cellWidth <= 0 || point.x() < kPadding || point.y() < kPadding) {
        return -1;
    }
    const int column = static_cast<int>((point.x() - kPadding) / (m_cellWidth + kGap));
    const int row = static_cast<int>((point.y() - kPadding) / (rowHeight() + kGap));
    if (column >= kColumns) {
        return -1;
    }
    const int index = row * kColumns + column;
    if (index >= m_cellCount || !cellRect(index).contains(point)) {
        return -1;
    }
    return index;
}

int GridGeometry::firstRowIn(qreal top) const
{
    return std::max(0, static_cast<int>(std::floor((top - kPadding) / (rowHeight() + kGap))));
}

int GridGeometry::lastRowIn(qreal bottom) const
{
    return std::min(rowCount() - 1, static_cast<int>(std::floor((bottom - kPadding) / (rowHeight() + kGap))));
}

}  // namespace sky
