#include "ui/editor_window/step_grid_view.hpp"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>

#include "core/translator.hpp"
#include "model/key_table.hpp"
#include "ui/editor_window/key_colors.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kBeatsPerBar = 4;
constexpr int kDotColumns = 5;
constexpr int kDotRows = 3;
constexpr qreal kTileRatio = 0.6;
constexpr qreal kTilePadding = 10;
constexpr qreal kDotGap = 4;
constexpr qreal kRadius = 8;
constexpr qreal kZoomFactor = 1.15;
constexpr qreal kDragThreshold = 5;
constexpr qreal kIdleDotShare = 0.2;
constexpr qreal kNoteShare = 0.9;
constexpr qreal kMinLetter = 14;
constexpr qreal kRestOpacity = 0.45;

QString keyLabel(int key)
{
    const std::string_view name = kAutoKeys[static_cast<std::size_t>(key)];
    return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())).toUpper();
}

QString captionTime(std::int64_t ms)
{
    return QStringLiteral("%1:%2.%3")
        .arg(ms / 60000)
        .arg((ms % 60000) / 1000, 2, 10, QLatin1Char('0'))
        .arg((ms % 1000) / 10, 2, 10, QLatin1Char('0'));
}

}  // namespace

StepGridView::StepGridView(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void StepGridView::setDocument(const EditorDocument *document)
{
    m_document = document;
    m_cursor = -1;
    m_first = -1;
    m_last = -1;
    m_drag = Drag::None;
    documentChanged();
}

void StepGridView::setGrid(const TimeGrid &grid)
{
    m_grid = grid;
    documentChanged();
}

void StepGridView::setSelection(int cursor, int first, int last)
{
    m_cursor = cursor;
    m_first = first;
    m_last = last;
    updateHeight();
    update();
}

void StepGridView::setPlayhead(double timeMs)
{
    const int column = timeMs >= 0 ? m_grid.columnOf(timeMs) : -1;
    if (column != m_playColumn) {
        m_playColumn = column;
        update();
    }
}

void StepGridView::documentChanged()
{
    updateHeight();
    update();
}

void StepGridView::zoomBy(int steps)
{
    m_targetWidth = std::clamp(m_targetWidth * std::pow(kZoomFactor, steps), kMinTileWidth, kMaxTileWidth);
    updateHeight();
    update();
}

int StepGridView::columnsPerRow() const
{
    return std::max(2, static_cast<int>((width() - kPadding * 2 + kGap) / (m_targetWidth + kGap)));
}

int StepGridView::tileCount() const
{
    const int last = m_document ? m_document->lastColumn(m_grid) : -1;
    const int used = std::max({last, m_cursor, m_last, 0}) + 1;
    const int perRow = columnsPerRow();
    return ((used + perRow - 1) / perRow + 1) * perRow;
}

qreal StepGridView::tileWidth() const
{
    const int perRow = columnsPerRow();
    return std::max<qreal>(1, (width() - kPadding * 2 - kGap * (perRow - 1)) / perRow);
}

qreal StepGridView::tileHeight() const
{
    return tileWidth() * kTileRatio;
}

qreal StepGridView::rowHeight() const
{
    return tileHeight() + kCaptionGap + kCaptionHeight;
}

QRectF StepGridView::tileRect(int column) const
{
    const int perRow = columnsPerRow();
    const int row = column / perRow;
    const int index = column % perRow;
    return QRectF(kPadding + index * (tileWidth() + kGap), kPadding + row * (rowHeight() + kGap), tileWidth(),
                  tileHeight());
}

QRectF StepGridView::captionRect(int column) const
{
    const QRectF tile = tileRect(column);
    return QRectF(tile.left() + 2, tile.bottom() + kCaptionGap, tile.width() - 4, kCaptionHeight);
}

QRect StepGridView::tileBounds(int column) const
{
    return tileRect(column).united(captionRect(column)).toAlignedRect().adjusted(-4, -4, 4, 4);
}

QRectF StepGridView::dotSlot(const QRectF &tile, int key) const
{
    const QRectF inner = tile.adjusted(kTilePadding, kTilePadding, -kTilePadding, -kTilePadding);
    const qreal width = (inner.width() - kDotGap * (kDotColumns - 1)) / kDotColumns;
    const qreal height = (inner.height() - kDotGap * (kDotRows - 1)) / kDotRows;
    const int column = key % kDotColumns;
    const int row = key / kDotColumns;
    return QRectF(inner.left() + column * (width + kDotGap), inner.top() + row * (height + kDotGap), width, height);
}

int StepGridView::tileAt(const QPointF &point) const
{
    const int perRow = columnsPerRow();
    const int index = static_cast<int>((point.x() - kPadding) / (tileWidth() + kGap));
    const int row = static_cast<int>((point.y() - kPadding) / (rowHeight() + kGap));
    if (point.x() < kPadding || point.y() < kPadding || index >= perRow) {
        return -1;
    }
    const int column = row * perRow + index;
    if (column >= tileCount() || !tileRect(column).united(captionRect(column)).contains(point)) {
        return -1;
    }
    return column;
}

int StepGridView::keyAt(const QRectF &tile, const QPointF &point) const
{
    for (int key = 0; key < EditorDocument::kKeyCount; ++key) {
        if (dotSlot(tile, key).adjusted(-kDotGap / 2, -kDotGap / 2, kDotGap / 2, kDotGap / 2).contains(point)) {
            return key;
        }
    }
    return -1;
}

void StepGridView::updateHeight()
{
    const int rows = tileCount() / columnsPerRow();
    const int height = static_cast<int>(std::ceil(kPadding * 2 + rows * rowHeight() + (rows - 1) * kGap));
    if (height != this->height()) {
        setFixedHeight(height);
    }
}

void StepGridView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateHeight();
}

void StepGridView::paintEvent(QPaintEvent *event)
{
    if (!m_document) {
        return;
    }
    const Theme &theme = currentTheme();
    const QColor accent = keyAccentColor();
    QPainter painter(this);
    preparePainter(painter);
    const int perRow = columnsPerRow();
    const int firstRow = std::max(0, static_cast<int>((event->rect().top() - kPadding) / (rowHeight() + kGap)));
    const int lastRow = static_cast<int>((event->rect().bottom() - kPadding) / (rowHeight() + kGap));
    const int count = tileCount();
    const int bar = m_grid.subdivision() * kBeatsPerBar;
    const QFont captionFont = FontLoader::font(11, QFont::Normal);
    const QFont barFont = FontLoader::font(11, QFont::Bold);
    const auto &notes = m_document->notes();

    for (int row = firstRow; row <= lastRow; ++row) {
        for (int index = 0; index < perRow; ++index) {
            const int column = row * perRow + index;
            if (column >= count) {
                break;
            }
            const QRectF tile = tileRect(column);
            const auto [begin, end] =
                m_document->indexRange(m_grid.columnBegin(column), m_grid.columnBegin(column + 1));
            std::array<std::int64_t, EditorDocument::kKeyCount> holds{};
            std::array<bool, EditorDocument::kKeyCount> active{};
            for (int i = begin; i < end; ++i) {
                const EditorNote &note = notes[static_cast<std::size_t>(i)];
                active[static_cast<std::size_t>(note.key)] = true;
                holds[static_cast<std::size_t>(note.key)] =
                    std::max(holds[static_cast<std::size_t>(note.key)], note.hold);
            }
            const bool rest = begin == end;
            const bool selected = m_first >= 0 && column >= m_first && column <= m_last;
            QColor fill = theme.gridColor;
            if (column == m_hover && m_drag == Drag::None) {
                fill = theme.gridHoverColor;
            }
            if (selected) {
                fill = blend(fill, accent, column == m_cursor ? 0.22 : 0.16);
            }
            painter.save();
            painter.setOpacity(rest && !selected && column != m_hover ? kRestOpacity : 1.0);
            painter.fillPath(roundedPath(tile, kRadius), fill);
            painter.restore();
            if (column == m_playColumn) {
                painter.save();
                painter.setPen(Qt::NoPen);
                painter.setBrush(withAlpha(accent, 0.16));
                painter.drawPath(roundedPath(tile.adjusted(-4, -4, 4, 4), kRadius + 4));
                painter.restore();
                painter.fillPath(roundedPath(tile, kRadius), withAlpha(accent, 0.14));
            }
            if (column == m_cursor || column == m_dropTile) {
                QPen pen(accent, 2);
                if (column == m_dropTile && column != m_cursor) {
                    pen.setStyle(Qt::DashLine);
                }
                painter.setPen(pen);
                painter.setBrush(Qt::NoBrush);
                painter.drawPath(roundedPath(tile.adjusted(1, 1, -1, -1), kRadius - 1));
            }

            painter.setPen(Qt::NoPen);
            for (int key = 0; key < EditorDocument::kKeyCount; ++key) {
                const QRectF slot = dotSlot(tile, key);
                const qreal side = std::min(slot.width(), slot.height());
                if (!active[static_cast<std::size_t>(key)]) {
                    const qreal dot = std::max<qreal>(3, side * kIdleDotShare);
                    painter.setBrush(theme.dotColor);
                    painter.drawEllipse(QRectF(slot.center().x() - dot / 2, slot.center().y() - dot / 2, dot, dot));
                    continue;
                }
                const QColor color = keyRowColor(key);
                const qreal size = side * kNoteShare;
                const QRectF note(slot.center().x() - size / 2, slot.center().y() - size / 2, size, size);
                painter.setBrush(color);
                painter.drawRoundedRect(note, size * 0.22, size * 0.22);
                if (holds[static_cast<std::size_t>(key)] > 0) {
                    painter.setBrush(blend(color, keyInkColor(color), 0.45));
                    painter.drawRoundedRect(QRectF(note.left() + 2, note.bottom() - 4, note.width() - 4, 3), 1.5, 1.5);
                }
                if (size >= kMinLetter) {
                    painter.setPen(keyInkColor(color));
                    painter.setFont(FontLoader::font(std::clamp(static_cast<int>(size * 0.5), 8, 14), QFont::Bold));
                    painter.drawText(note, Qt::AlignCenter, keyLabel(key));
                    painter.setPen(Qt::NoPen);
                }
            }

            const bool barStart = column % bar == 0;
            painter.setFont(barStart ? barFont : captionFont);
            painter.setPen(barStart ? theme.fontColor : blend(theme.background, theme.fontColor, 0.65));
            const QString time = captionTime(m_grid.timeOf(column));
            painter.drawText(captionRect(column), Qt::AlignLeft | Qt::AlignVCenter,
                             barStart ? tr("Bar %1  ·  %2").arg(column / bar + 1).arg(time) : time);
        }
    }
}

void StepGridView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_document) {
        return;
    }
    const QPointF point = event->position();
    const int tile = tileAt(point);
    if (tile < 0) {
        return;
    }
    m_pressPoint = point;
    m_pressTile = tile;
    m_pressKey = -1;
    m_dropTile = -1;
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    const bool single = m_first == m_last && m_cursor == tile;
    if (!shift && single && m_editable) {
        m_pressKey = keyAt(tileRect(tile), point);
        m_drag = Drag::Move;
        return;
    }
    m_drag = Drag::Select;
    emit columnPressed(tile, shift);
}

void StepGridView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF point = event->position();
    if (m_drag == Drag::None) {
        const int tile = tileAt(point);
        if (tile != m_hover) {
            m_hover = tile;
            update();
        }
        return;
    }
    if ((point - m_pressPoint).manhattanLength() < kDragThreshold) {
        return;
    }
    const int tile = tileAt(point);
    if (m_drag == Drag::Select) {
        if (tile >= 0) {
            emit columnPressed(tile, true);
        }
        return;
    }
    setCursor(Qt::ClosedHandCursor);
    if (tile != m_dropTile) {
        m_dropTile = tile;
        update();
    }
}

void StepGridView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const Drag drag = m_drag;
    const bool moved = (event->position() - m_pressPoint).manhattanLength() >= kDragThreshold;
    m_drag = Drag::None;
    setCursor(Qt::ArrowCursor);
    if (drag == Drag::Move) {
        if (moved && m_dropTile >= 0 && m_dropTile != m_pressTile) {
            emit columnsDragged(m_pressTile, m_dropTile);
        } else if (!moved && m_pressKey >= 0) {
            emit cellToggled(m_pressTile, m_pressKey);
        }
    }
    m_dropTile = -1;
    update();
}

void StepGridView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const int steps = event->angleDelta().y() / 120;
        if (steps != 0) {
            zoomBy(steps);
        }
        event->accept();
        return;
    }
    event->ignore();
}

void StepGridView::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hover = -1;
    update();
}

}  // namespace sky
