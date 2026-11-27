#pragma once

#include <QWidget>

#include "ui/editor_window/editor_document.hpp"

namespace sky {

class StepGridView final : public QWidget {
    Q_OBJECT

public:
    static constexpr qreal kPadding = 6;
    static constexpr qreal kGap = 12;
    static constexpr qreal kCaptionGap = 4;
    static constexpr qreal kCaptionHeight = 18;
    static constexpr qreal kMinTileWidth = 120;
    static constexpr qreal kMaxTileWidth = 320;

    explicit StepGridView(QWidget *parent = nullptr);

    void setDocument(const EditorDocument *document);
    void setGrid(const TimeGrid &grid);
    void setSelection(int cursor, int first, int last);
    void setEditable(bool editable) { m_editable = editable; }
    void setPlayhead(double timeMs);
    void documentChanged();
    void zoomBy(int steps);
    int columnsPerRow() const;
    QRect tileBounds(int column) const;

signals:
    void columnPressed(int column, bool extend);
    void cellToggled(int column, int key);
    void columnsDragged(int from, int to);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class Drag { None, Select, Move };

    int tileCount() const;
    qreal tileWidth() const;
    qreal tileHeight() const;
    qreal rowHeight() const;
    QRectF tileRect(int column) const;
    QRectF captionRect(int column) const;
    QRectF dotSlot(const QRectF &tile, int key) const;
    int tileAt(const QPointF &point) const;
    int keyAt(const QRectF &tile, const QPointF &point) const;
    void updateHeight();

    const EditorDocument *m_document = nullptr;
    TimeGrid m_grid;
    bool m_editable = true;
    int m_cursor = -1;
    int m_first = -1;
    int m_last = -1;
    int m_hover = -1;
    int m_playColumn = -1;
    qreal m_targetWidth = 190;
    Drag m_drag = Drag::None;
    QPointF m_pressPoint;
    int m_pressTile = -1;
    int m_pressKey = -1;
    int m_dropTile = -1;
};

}  // namespace sky
