#include "ui/editor_window/note_grid.hpp"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "ui/core/easing.hpp"
#include "ui/editor_window/editor_document.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr qreal kRadius = 8;
constexpr qreal kIdleDot = 3;
constexpr qreal kActiveDot = 12;
constexpr qreal kActiveRadius = 2;
constexpr qreal kGlowRing = 2;
constexpr int kDotMs = 200;
constexpr int kEntranceMs = 200;
constexpr int kStaggerMs = 50;
constexpr qreal kEntranceScale = 0.8;
constexpr int kFrameMs = 16;

QString timestamp(double timeMs)
{
    const auto ms = static_cast<qint64>(timeMs);
    return QStringLiteral("%1.%2s").arg(ms / 1000).arg(ms % 1000, 3, 10, QLatin1Char('0'));
}

}  // namespace

NoteGrid::NoteGrid(QWidget *parent) : QWidget(parent), m_ticker(new QTimer(this))
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    m_ticker->setInterval(kFrameMs);
    connect(m_ticker, &QTimer::timeout, this, &NoteGrid::tick);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

GridGeometry NoteGrid::geometry() const
{
    return GridGeometry(width(), m_document ? m_document->cellCount() : 0);
}

void NoteGrid::setDocument(const EditorDocument *document, int visibleHeight)
{
    m_document = document;
    m_selected = -1;
    m_hover = -1;
    m_dots.clear();
    const GridGeometry grid = geometry();
    const int visibleRows = grid.rowHeight() > 0 ? static_cast<int>(std::ceil(visibleHeight / (grid.rowHeight() + GridGeometry::kGap))) + 1 : 4;
    m_staggered = std::min(document ? document->cellCount() : 0, visibleRows * GridGeometry::kColumns);
    m_clock.restart();
    m_ticker->start();
    updateHeight();
    update();
}

void NoteGrid::setBottomPadding(int padding)
{
    m_bottomPadding = padding;
    updateHeight();
}

void NoteGrid::updateHeight()
{
    setFixedHeight(static_cast<int>(std::ceil(geometry().contentHeight())) + m_bottomPadding);
}

void NoteGrid::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int wanted = static_cast<int>(std::ceil(geometry().contentHeight())) + m_bottomPadding;
    if (height() != wanted) {
        setFixedHeight(wanted);
    }
}

QRect NoteGrid::cellBounds(int cell) const
{
    const GridGeometry grid = geometry();
    return grid.cellRect(cell).united(grid.captionRect(cell)).toAlignedRect().adjusted(-2, -2, 2, 2);
}

void NoteGrid::setSelected(int cell)
{
    const int previous = m_selected;
    m_selected = cell;
    if (previous >= 0) {
        update(cellBounds(previous));
    }
    if (cell >= 0) {
        update(cellBounds(cell));
    }
}

void NoteGrid::refreshCell(int cell)
{
    if (cell >= 0) {
        update(cellBounds(cell));
    }
}

void NoteGrid::animateDot(int cell, int dot)
{
    std::erase_if(m_dots, [&](const DotAnimation &a) { return a.cell == cell && a.dot == dot; });
    m_dots.push_back(DotAnimation{cell, dot, m_clock.elapsed()});
    m_ticker->start();
    refreshCell(cell);
}

qreal NoteGrid::entrance(int cell) const
{
    if (cell >= m_staggered) {
        return 1.0;
    }
    const qreal t = (m_clock.elapsed() - static_cast<qint64>(cell) * kStaggerMs) / static_cast<qreal>(kEntranceMs);
    return easingCurve(Easing::Ease).valueForProgress(std::clamp<qreal>(t, 0.0, 1.0));
}

qreal NoteGrid::dotProgress(int cell, int dot, bool active) const
{
    for (const DotAnimation &a : m_dots) {
        if (a.cell == cell && a.dot == dot) {
            const qreal t = std::clamp<qreal>((m_clock.elapsed() - a.start) / static_cast<qreal>(kDotMs), 0.0, 1.0);
            const qreal eased = easingCurve(Easing::Ease).valueForProgress(t);
            return active ? eased : 1.0 - eased;
        }
    }
    return active ? 1.0 : 0.0;
}

void NoteGrid::tick()
{
    const qint64 now = m_clock.elapsed();
    std::vector<int> cells;
    for (const DotAnimation &a : m_dots) {
        cells.push_back(a.cell);
    }
    std::erase_if(m_dots, [now](const DotAnimation &a) { return now - a.start > kDotMs; });
    const bool entering = m_staggered > 0 && now <= static_cast<qint64>(m_staggered) * kStaggerMs + kEntranceMs;
    if (entering) {
        update();
    } else {
        for (int cell : cells) {
            refreshCell(cell);
        }
    }
    if (!entering && m_dots.empty()) {
        m_ticker->stop();
    }
}

void NoteGrid::mouseMoveEvent(QMouseEvent *event)
{
    const int cell = geometry().cellAt(event->position());
    if (cell != m_hover) {
        const int previous = m_hover;
        m_hover = cell;
        refreshCell(previous);
        refreshCell(cell);
    }
}

void NoteGrid::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    const int previous = m_hover;
    m_hover = -1;
    refreshCell(previous);
}

void NoteGrid::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const int cell = geometry().cellAt(event->position());
    if (cell >= 0) {
        emit cellClicked(cell);
    }
}

void NoteGrid::paintEvent(QPaintEvent *event)
{
    if (!m_document) {
        return;
    }
    const Theme &theme = currentTheme();
    const EditorPalette &e = theme.editor;
    QPainter painter(this);
    preparePainter(painter);
    const GridGeometry grid = geometry();
    const int firstRow = grid.firstRowIn(event->rect().top());
    const int lastRow = grid.lastRowIn(event->rect().bottom());
    const QFont caption = FontLoader::font(11, QFont::Light);

    for (int row = firstRow; row <= lastRow; ++row) {
        for (int column = 0; column < GridGeometry::kColumns; ++column) {
            const int cell = row * GridGeometry::kColumns + column;
            if (cell >= m_document->cellCount()) {
                break;
            }
            const qreal presence = entrance(cell);
            if (presence <= 0) {
                continue;
            }
            const QRectF rect = grid.cellRect(cell);
            painter.save();
            painter.setOpacity(presence);
            const qreal scale = kEntranceScale + (1.0 - kEntranceScale) * presence;
            const QPointF center = rect.united(grid.captionRect(cell)).center();
            painter.translate(center);
            painter.scale(scale, scale);
            painter.translate(-center);

            QColor fill = theme.gridColor;
            if (cell == m_selected) {
                fill = theme.activeColor;
            } else if (cell == m_hover) {
                fill = theme.gridHoverColor;
            }
            painter.fillPath(roundedPath(rect, kRadius), fill);

            for (int dot = 0; dot < 15; ++dot) {
                const QRectF slot = grid.dotSlot(rect, dot);
                const bool active = m_document->hasKey(cell, EditorDocument::keyAt(dot));
                const qreal p = dotProgress(cell, dot, active);
                const qreal size = kIdleDot + (kActiveDot - kIdleDot) * p;
                const QRectF dotRect(slot.center().x() - size / 2, slot.center().y() - size / 2, size, size);
                if (p > 0.5 && e.noteGlow) {
                    painter.save();
                    painter.setOpacity(painter.opacity() * (p - 0.5) * 2);
                    ShadowPainter::paint(painter, dotRect, kActiveRadius, e.glowDrop);
                    painter.fillPath(roundedPath(dotRect.adjusted(-kGlowRing, -kGlowRing, kGlowRing, kGlowRing),
                                                 kActiveRadius + kGlowRing),
                                     e.glowRing);
                    painter.restore();
                }
                const QColor color = blend(theme.dotColor, theme.gridNoteColor, p);
                const qreal radius = size / 2 + (kActiveRadius - size / 2) * p;
                painter.fillPath(roundedPath(dotRect, radius), color);
            }

            painter.setFont(caption);
            painter.setPen(theme.fontColor);
            painter.drawText(grid.captionRect(cell), Qt::AlignCenter, timestamp(m_document->timeAt(cell)));
            painter.restore();
        }
    }
}

}  // namespace sky
