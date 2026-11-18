#pragma once

#include <vector>

#include <QElapsedTimer>
#include <QWidget>

#include "ui/editor_window/grid_geometry.hpp"

class QTimer;

namespace sky {

class EditorDocument;

class NoteGrid final : public QWidget {
    Q_OBJECT

public:
    explicit NoteGrid(QWidget *parent = nullptr);

    void setDocument(const EditorDocument *document, int visibleHeight);
    void setSelected(int cell);
    int selected() const { return m_selected; }
    void animateDot(int cell, int dot);
    void refreshCell(int cell);
    void setBottomPadding(int padding);
    QRect cellBounds(int cell) const;

signals:
    void cellClicked(int cell);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct DotAnimation {
        int cell;
        int dot;
        qint64 start;
    };

    GridGeometry geometry() const;
    void updateHeight();
    qreal entrance(int cell) const;
    qreal dotProgress(int cell, int dot, bool active) const;
    void tick();

    const EditorDocument *m_document = nullptr;
    int m_hover = -1;
    int m_selected = -1;
    int m_bottomPadding = 0;
    int m_staggered = 0;
    QElapsedTimer m_clock;
    std::vector<DotAnimation> m_dots;
    QTimer *m_ticker;
};

}  // namespace sky
