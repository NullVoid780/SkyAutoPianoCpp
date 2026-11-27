#pragma once

#include <QPixmap>
#include <QWidget>

#include "ui/editor_window/editor_document.hpp"

namespace sky {

class TimelineView final : public QWidget {
    Q_OBJECT

public:
    static constexpr int kGutter = 40;
    static constexpr int kRuler = 26;
    static constexpr int kMinimap = 30;
    static constexpr int kMinimapGap = 6;
    static constexpr int kTailColumns = 32;
    static constexpr qreal kMinColumnWidth = 10;
    static constexpr qreal kMaxColumnWidth = 72;

    explicit TimelineView(QWidget *parent = nullptr);

    void setDocument(const EditorDocument *document);
    void setGrid(const TimeGrid &grid);
    void setSelection(int cursor, int first, int last);
    void setEditable(bool editable) { m_editable = editable; }
    void setPlayhead(double timeMs);
    void documentChanged();
    void ensureColumnVisible(int column);
    void zoomBy(int steps);
    int visibleColumns() const;

    QSize sizeHint() const override { return {800, 300}; }
    QSize minimumSizeHint() const override { return {360, kRuler + kMinimap + kMinimapGap + 15 * 13}; }

signals:
    void columnPressed(int column, bool extend);
    void cellToggled(int column, int key);
    void noteMoved(qint64 time, int key, qint64 newTime, int newKey);
    void holdChanged(qint64 time, int key, qint64 hold);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class Drag { None, Ruler, Minimap, Note, Hold, Cell };

    QRectF rollRect() const;
    QRectF rulerRect() const;
    QRectF minimapRect() const;
    qreal rowHeight() const;
    int contentColumns() const;
    qreal contentWidth() const;
    qreal maxScroll() const;
    void setScroll(qreal scroll);
    qreal xOfNote(std::int64_t time) const;
    qreal xOfTime(double time) const;
    double timeAtX(qreal x) const;
    int columnAtX(qreal x) const;
    int keyAtY(qreal y) const;
    qreal yOfKey(int key) const;
    qreal headSize() const;
    int noteAt(const QPointF &point, bool &onHold) const;
    std::int64_t dragTime(const QPointF &point, bool free) const;
    void scrollMinimapTo(qreal x);
    void paintGrid(QPainter &painter, const QRectF &roll, int firstColumn, int lastColumn);
    void paintNotes(QPainter &painter, const QRectF &roll);
    void paintRuler(QPainter &painter, int firstColumn, int lastColumn);
    void paintGutter(QPainter &painter);
    void paintMinimap(QPainter &painter);
    void paintPlayhead(QPainter &painter, const QRectF &roll);
    void rebuildMinimap();

    const EditorDocument *m_document = nullptr;
    TimeGrid m_grid;
    bool m_editable = true;
    int m_cursor = -1;
    int m_first = -1;
    int m_last = -1;
    int m_hoverColumn = -1;
    int m_hoverKey = -1;
    double m_playhead = -1;
    qreal m_columnWidth = 30;
    qreal m_scroll = 0;
    std::int64_t m_maxHold = 0;
    std::vector<int> m_cursorKeys;

    Drag m_drag = Drag::None;
    QPointF m_pressPoint;
    bool m_dragMoved = false;
    int m_pressColumn = -1;
    int m_pressKey = -1;
    EditorNote m_pressNote;
    std::int64_t m_ghostTime = 0;
    int m_ghostKey = -1;
    std::int64_t m_ghostHold = 0;

    QPixmap m_minimapCache;
    bool m_minimapDirty = true;
};

}  // namespace sky
