#include "ui/editor_window/timeline_view.hpp"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "model/key_table.hpp"
#include "ui/editor_window/key_colors.hpp"
#include "ui/render/font_loader.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/theme_manager.hpp"

namespace sky {

namespace {

constexpr int kBeatsPerBar = 4;
constexpr qreal kDragThreshold = 4;
constexpr qreal kZoomFactor = 1.2;
constexpr qreal kScrollColumnsPerNotch = 4;
constexpr qreal kMinLabelSpacing = 56;
constexpr qreal kCursorWidth = 2;
constexpr qreal kHoldThickness = 0.42;
constexpr qreal kTileShare = 0.86;
constexpr qreal kMinTile = 8;
constexpr qreal kMinLetterTile = 15;
constexpr qreal kTileRadius = 5;
constexpr qreal kRadius = 6;
const QColor kAccent = keyAccentColor();

QColor rowColor(int key)
{
    return keyRowColor(key);
}

QString keyLabel(int key)
{
    const std::string_view name = kAutoKeys[static_cast<std::size_t>(key)];
    return QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())).toUpper();
}

QString rulerText(std::int64_t ms, bool tenths)
{
    const std::int64_t minutes = ms / 60000;
    const std::int64_t rest = ms % 60000;
    QString text = QStringLiteral("%1:%2").arg(minutes).arg(rest / 1000, 2, 10, QLatin1Char('0'));
    if (tenths) {
        text += QStringLiteral(".%1").arg((rest % 1000) / 100);
    }
    return text;
}

}  // namespace

TimelineView::TimelineView(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        m_minimapDirty = true;
        update();
    });
}

void TimelineView::setDocument(const EditorDocument *document)
{
    m_document = document;
    m_scroll = 0;
    m_cursor = -1;
    m_first = -1;
    m_last = -1;
    m_drag = Drag::None;
    documentChanged();
}

void TimelineView::setGrid(const TimeGrid &grid)
{
    m_grid = grid;
    documentChanged();
}

void TimelineView::setSelection(int cursor, int first, int last)
{
    m_cursor = cursor;
    m_first = first;
    m_last = last;
    m_cursorKeys = m_document && cursor >= 0 ? m_document->keysInColumn(cursor, m_grid) : std::vector<int>{};
    update();
}

void TimelineView::setPlayhead(double timeMs)
{
    m_playhead = timeMs;
    update();
}

void TimelineView::documentChanged()
{
    m_maxHold = 0;
    if (m_document) {
        for (const EditorNote &note : m_document->notes()) {
            m_maxHold = std::max(m_maxHold, note.hold);
        }
    }
    m_cursorKeys = m_document && m_cursor >= 0 ? m_document->keysInColumn(m_cursor, m_grid) : std::vector<int>{};
    m_minimapDirty = true;
    setScroll(m_scroll);
    update();
}

void TimelineView::ensureColumnVisible(int column)
{
    if (column < 0) {
        return;
    }
    const qreal width = rollRect().width();
    const qreal left = column * m_columnWidth;
    const qreal right = left + m_columnWidth;
    const qreal margin = std::min(m_columnWidth * 2, width / 4);
    if (left < m_scroll) {
        setScroll(left - margin);
    } else if (right > m_scroll + width) {
        setScroll(right - width + margin);
    }
}

void TimelineView::zoomBy(int steps)
{
    const QRectF roll = rollRect();
    const qreal anchorX = m_cursor >= 0 ? xOfTime(static_cast<double>(m_grid.timeOf(m_cursor))) : roll.center().x();
    const double anchorTime = timeAtX(std::clamp(anchorX, roll.left(), roll.right()));
    const qreal before = std::clamp(anchorX, roll.left(), roll.right());
    m_columnWidth = std::clamp(m_columnWidth * std::pow(kZoomFactor, steps), kMinColumnWidth, kMaxColumnWidth);
    m_minimapDirty = true;
    setScroll(((anchorTime - m_grid.originMs()) / m_grid.slotMs() + 0.5) * m_columnWidth - (before - roll.left()));
}

int TimelineView::visibleColumns() const
{
    return std::max(1, static_cast<int>(rollRect().width() / m_columnWidth));
}

QRectF TimelineView::rollRect() const
{
    const qreal bottom = height() - kMinimap - kMinimapGap;
    return QRectF(kGutter, kRuler, std::max<qreal>(0, width() - kGutter), std::max<qreal>(0, bottom - kRuler));
}

QRectF TimelineView::rulerRect() const
{
    return QRectF(kGutter, 0, std::max<qreal>(0, width() - kGutter), kRuler);
}

QRectF TimelineView::minimapRect() const
{
    return QRectF(kGutter, height() - kMinimap, std::max<qreal>(0, width() - kGutter), kMinimap);
}

qreal TimelineView::rowHeight() const
{
    return rollRect().height() / EditorDocument::kKeyCount;
}

int TimelineView::contentColumns() const
{
    const int last = m_document ? m_document->lastColumn(m_grid) : -1;
    return std::max({last, m_cursor, m_last, 0}) + 1 + kTailColumns;
}

qreal TimelineView::contentWidth() const
{
    return contentColumns() * m_columnWidth;
}

qreal TimelineView::maxScroll() const
{
    return std::max<qreal>(0, contentWidth() - rollRect().width());
}

void TimelineView::setScroll(qreal scroll)
{
    const qreal clamped = std::clamp(scroll, qreal(0), maxScroll());
    if (clamped != m_scroll) {
        m_scroll = clamped;
        update();
    }
}

qreal TimelineView::xOfNote(std::int64_t time) const
{
    return kGutter + (m_grid.columnOf(static_cast<double>(time)) + 0.5) * m_columnWidth - m_scroll;
}

qreal TimelineView::xOfTime(double time) const
{
    return kGutter + ((time - m_grid.originMs()) / m_grid.slotMs() + 0.5) * m_columnWidth - m_scroll;
}

double TimelineView::timeAtX(qreal x) const
{
    return m_grid.originMs() + ((x - kGutter + m_scroll) / m_columnWidth - 0.5) * m_grid.slotMs();
}

int TimelineView::columnAtX(qreal x) const
{
    return std::max(0, static_cast<int>(std::floor((x - kGutter + m_scroll) / m_columnWidth)));
}

int TimelineView::keyAtY(qreal y) const
{
    const QRectF roll = rollRect();
    const int row = std::clamp(static_cast<int>((y - roll.top()) / rowHeight()), 0, EditorDocument::kKeyCount - 1);
    return EditorDocument::kKeyCount - 1 - row;
}

qreal TimelineView::yOfKey(int key) const
{
    const int row = EditorDocument::kKeyCount - 1 - key;
    return rollRect().top() + (row + 0.5) * rowHeight();
}

qreal TimelineView::headSize() const
{
    return std::max(kMinTile, std::min(rowHeight(), m_columnWidth) * kTileShare);
}

int TimelineView::noteAt(const QPointF &point, bool &onHold) const
{
    onHold = false;
    if (!m_document) {
        return -1;
    }
    const int key = keyAtY(point.y());
    const double time = timeAtX(point.x());
    const qreal reach = headSize() / 2 + 3;
    const double reachMs = reach / m_columnWidth * m_grid.slotMs();
    const auto [first, end] = m_document->indexRange(static_cast<std::int64_t>(std::floor(time - reachMs - m_maxHold)),
                                                     static_cast<std::int64_t>(std::ceil(time + reachMs)) + 1);
    const auto &notes = m_document->notes();
    int holdHit = -1;
    for (int i = end - 1; i >= first; --i) {
        const EditorNote &note = notes[static_cast<std::size_t>(i)];
        if (note.key != key) {
            continue;
        }
        const qreal x = xOfNote(note.time);
        if (std::fabs(point.x() - x) <= reach) {
            return i;
        }
        if (note.hold > 0 && holdHit < 0 && point.x() > x &&
            point.x() <= x + static_cast<qreal>(note.hold) / m_grid.slotMs() * m_columnWidth + 4) {
            holdHit = i;
        }
    }
    onHold = holdHit >= 0;
    return holdHit;
}

std::int64_t TimelineView::dragTime(const QPointF &point, bool free) const
{
    const double time = std::max(0.0, timeAtX(point.x()));
    return free ? std::llround(time) : m_grid.timeOf(m_grid.columnOf(time));
}

void TimelineView::scrollMinimapTo(qreal x)
{
    const QRectF map = minimapRect();
    if (map.width() <= 0) {
        return;
    }
    const qreal share = std::clamp((x - map.left()) / map.width(), qreal(0), qreal(1));
    setScroll(share * contentWidth() - rollRect().width() / 2);
}

void TimelineView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_minimapDirty = true;
    setScroll(m_scroll);
}

void TimelineView::paintEvent(QPaintEvent *)
{
    const Theme &theme = currentTheme();
    QPainter painter(this);
    preparePainter(painter);
    painter.fillRect(rect(), theme.background);
    const QRectF roll = rollRect();
    if (roll.width() <= 0 || roll.height() <= 0) {
        return;
    }
    const int firstColumn = columnAtX(roll.left());
    const int lastColumn = columnAtX(roll.right());

    painter.save();
    painter.setClipPath(roundedPath(roll, kRadius));
    paintGrid(painter, roll, firstColumn, lastColumn);
    paintNotes(painter, roll);
    painter.restore();
    paintPlayhead(painter, roll);

    paintRuler(painter, firstColumn, lastColumn);
    paintGutter(painter);
    paintMinimap(painter);
}

void TimelineView::paintGrid(QPainter &painter, const QRectF &roll, int firstColumn, int lastColumn)
{
    const Theme &theme = currentTheme();
    const qreal row = rowHeight();
    painter.fillRect(roll, theme.gridColor);
    const int bar = m_grid.subdivision() * kBeatsPerBar;

    for (int column = firstColumn; column <= lastColumn; ++column) {
        const qreal x = kGutter + column * m_columnWidth - m_scroll;
        if ((column / bar) % 2 == 1) {
            painter.fillRect(QRectF(x, roll.top(), m_columnWidth, roll.height()), blend(theme.gridColor, theme.fontColor, 0.035));
        }
    }
    for (int key = 0; key < EditorDocument::kKeyCount; key += 7) {
        const qreal y = yOfKey(key) - row / 2;
        painter.fillRect(QRectF(roll.left(), y, roll.width(), row), blend(theme.gridColor, theme.activeColor, 0.10));
    }
    if (m_first >= 0) {
        const qreal x = kGutter + m_first * m_columnWidth - m_scroll;
        painter.fillRect(QRectF(x, roll.top(), (m_last - m_first + 1) * m_columnWidth, roll.height()),
                         withAlpha(kAccent, 0.16));
    }
    if (m_editable && m_hoverColumn >= 0 && m_hoverKey >= 0 && m_drag == Drag::None) {
        const qreal x = kGutter + m_hoverColumn * m_columnWidth - m_scroll;
        painter.fillRect(QRectF(x, yOfKey(m_hoverKey) - row / 2, m_columnWidth, row), theme.gridHoverColor);
    }

    QPen line(blend(theme.gridColor, theme.fontColor, 0.08), 1);
    painter.setPen(line);
    for (int key = 1; key < EditorDocument::kKeyCount; ++key) {
        const qreal y = std::round(roll.top() + key * row) + 0.5;
        painter.drawLine(QPointF(roll.left(), y), QPointF(roll.right(), y));
    }
    for (int column = firstColumn; column <= lastColumn + 1; ++column) {
        const qreal x = std::round(kGutter + column * m_columnWidth - m_scroll) + 0.5;
        qreal strength = 0.07;
        if (column % bar == 0) {
            strength = 0.34;
        } else if (column % m_grid.subdivision() == 0) {
            strength = 0.17;
        }
        line.setColor(blend(theme.gridColor, theme.fontColor, strength));
        painter.setPen(line);
        painter.drawLine(QPointF(x, roll.top()), QPointF(x, roll.bottom()));
    }

    if (m_cursor >= 0) {
        const qreal x = kGutter + m_cursor * m_columnWidth - m_scroll;
        painter.fillRect(QRectF(x, roll.top(), m_columnWidth, roll.height()), withAlpha(kAccent, 0.10));
        painter.setPen(QPen(kAccent, kCursorWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(roundedPath(QRectF(x + 1, roll.top() + 1, m_columnWidth - 2, roll.height() - 2), 4));
    }
}

void TimelineView::paintNotes(QPainter &painter, const QRectF &roll)
{
    if (!m_document) {
        return;
    }
    const qreal row = rowHeight();
    const qreal tile = headSize();
    const qreal thickness = std::max<qreal>(4, row * kHoldThickness);
    const double from = timeAtX(roll.left() - tile) - static_cast<double>(m_maxHold);
    const double to = timeAtX(roll.right() + tile);
    const auto [first, end] = m_document->indexRange(static_cast<std::int64_t>(std::floor(from)),
                                                     static_cast<std::int64_t>(std::ceil(to)) + 1);
    const auto &notes = m_document->notes();
    const bool dragging = (m_drag == Drag::Note || m_drag == Drag::Hold) && m_dragMoved;
    const bool letters = tile >= kMinLetterTile;
    const QFont letterFont = FontLoader::font(std::clamp(static_cast<int>(tile * 0.55), 9, 15), QFont::Bold);
    painter.setFont(letterFont);

    const auto paintNote = [&](std::int64_t time, int key, std::int64_t hold, bool ghost) {
        const QColor color = rowColor(key);
        const qreal x = xOfNote(time);
        const qreal y = yOfKey(key);
        if (hold > 0) {
            const qreal endX = x + static_cast<qreal>(hold) / m_grid.slotMs() * m_columnWidth;
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(color, ghost ? 0.35 : 0.5));
            painter.drawRoundedRect(QRectF(x, y - thickness / 2, std::max<qreal>(endX - x, thickness), thickness),
                                    thickness / 2, thickness / 2);
            painter.setBrush(color);
            painter.drawRoundedRect(QRectF(endX - 2.5, y - thickness * 0.75, 5, thickness * 1.5), 2.5, 2.5);
        }
        const QRectF rect(x - tile / 2, y - tile / 2, tile, tile);
        if (ghost) {
            painter.setPen(QPen(kAccent, 2));
            painter.setBrush(withAlpha(color, 0.45));
        } else {
            painter.setPen(QPen(color.darker(125), 1));
            painter.setBrush(color);
        }
        painter.drawRoundedRect(rect, kTileRadius, kTileRadius);
        if (letters) {
            painter.setPen(keyInkColor(color));
            painter.drawText(rect, Qt::AlignCenter, keyLabel(key));
        }
    };

    const int playColumn = m_playhead >= 0 ? m_grid.columnOf(m_playhead) : -1;
    for (int i = first; i < end; ++i) {
        const EditorNote &note = notes[static_cast<std::size_t>(i)];
        if (dragging && note == m_pressNote) {
            continue;
        }
        if (playColumn >= 0 && m_grid.columnOf(static_cast<double>(note.time)) == playColumn) {
            const qreal x = xOfNote(note.time);
            const qreal y = yOfKey(note.key);
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(kAccent, 0.35));
            painter.drawRoundedRect(QRectF(x - tile / 2 - 4, y - tile / 2 - 4, tile + 8, tile + 8), kTileRadius + 3,
                                    kTileRadius + 3);
        }
        paintNote(note.time, note.key, note.hold, false);
    }
    if (dragging) {
        paintNote(m_ghostTime, m_ghostKey, m_ghostHold, true);
    }
}

void TimelineView::paintPlayhead(QPainter &painter, const QRectF &roll)
{
    if (m_playhead < 0) {
        return;
    }
    const qreal x = xOfTime(m_playhead);
    if (x < roll.left() || x > roll.right()) {
        return;
    }
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.fillRect(QRectF(x - 3, roll.top(), 6, roll.height()), withAlpha(kAccent, 0.18));
    painter.fillRect(QRectF(x - 1, roll.top(), 2, roll.height()), kAccent);
    QPolygonF marker;
    marker << QPointF(x - 6, roll.top() - 9) << QPointF(x + 6, roll.top() - 9) << QPointF(x, roll.top() - 1);
    painter.setBrush(kAccent);
    painter.drawPolygon(marker);
    painter.restore();
}

void TimelineView::paintRuler(QPainter &painter, int firstColumn, int lastColumn)
{
    const Theme &theme = currentTheme();
    const QRectF ruler = rulerRect();
    painter.fillRect(ruler, theme.background);
    painter.save();
    painter.setClipRect(ruler);
    const int bar = m_grid.subdivision() * kBeatsPerBar;
    int barsPerLabel = 1;
    while (barsPerLabel * bar * m_columnWidth < kMinLabelSpacing) {
        barsPerLabel *= 2;
    }
    const bool tenths = m_grid.slotMs() * bar * barsPerLabel < 1000.0;
    painter.setFont(FontLoader::font(12, QFont::Medium));
    for (int column = firstColumn; column <= lastColumn; ++column) {
        const qreal x = kGutter + column * m_columnWidth - m_scroll;
        if (column % bar == 0) {
            painter.setPen(QPen(blend(theme.background, theme.fontColor, 0.45), 1));
            painter.drawLine(QPointF(x + 0.5, ruler.bottom() - 7), QPointF(x + 0.5, ruler.bottom()));
            if ((column / bar) % barsPerLabel == 0) {
                painter.setPen(blend(theme.background, theme.fontColor, 0.85));
                painter.drawText(QRectF(x + 4, ruler.top(), kMinLabelSpacing, ruler.height() - 3),
                                 Qt::AlignLeft | Qt::AlignVCenter, rulerText(m_grid.timeOf(column), tenths));
            }
        } else if (column % m_grid.subdivision() == 0) {
            painter.setPen(QPen(blend(theme.background, theme.fontColor, 0.25), 1));
            painter.drawLine(QPointF(x + 0.5, ruler.bottom() - 4), QPointF(x + 0.5, ruler.bottom()));
        }
    }
    if (m_first >= 0) {
        const qreal x = kGutter + m_first * m_columnWidth - m_scroll;
        painter.fillRect(QRectF(x, ruler.bottom() - 3, (m_last - m_first + 1) * m_columnWidth, 3), kAccent);
    }
    painter.restore();
}

void TimelineView::paintGutter(QPainter &painter)
{
    const Theme &theme = currentTheme();
    painter.fillRect(QRectF(0, 0, kGutter, height()), theme.background);
    const qreal row = rowHeight();
    const bool caps = row >= 16;
    const qreal cap = caps ? std::min(row - 4, qreal(24)) : row;
    const QFont font = FontLoader::font(std::clamp(static_cast<int>(cap * 0.6), 8, 13), QFont::Bold);
    painter.setFont(font);
    for (int key = 0; key < EditorDocument::kKeyCount; ++key) {
        const bool active = std::find(m_cursorKeys.begin(), m_cursorKeys.end(), key) != m_cursorKeys.end();
        const QColor color = rowColor(key);
        const qreal y = yOfKey(key);
        const QRectF rect(kGutter - cap - 6, y - cap / 2, cap, cap);
        if (!caps) {
            painter.setPen(active ? color : blend(theme.background, theme.fontColor, 0.75));
            painter.drawText(QRectF(0, y - row / 2, kGutter - 8, row), Qt::AlignRight | Qt::AlignVCenter,
                             keyLabel(key));
            continue;
        }
        painter.setPen(active ? Qt::NoPen : QPen(withAlpha(color, 0.7), 1.2));
        painter.setBrush(active ? color : withAlpha(color, 0.12));
        painter.drawRoundedRect(rect, 4, 4);
        painter.setPen(active ? keyInkColor(color) : blend(theme.background, theme.fontColor, 0.9));
        painter.drawText(rect, Qt::AlignCenter, keyLabel(key));
    }
}

void TimelineView::rebuildMinimap()
{
    const Theme &theme = currentTheme();
    const QRectF map = minimapRect();
    const qreal ratio = devicePixelRatioF();
    m_minimapCache = QPixmap(QSize(std::max(1, static_cast<int>(map.width() * ratio)),
                                   std::max(1, static_cast<int>(map.height() * ratio))));
    m_minimapCache.setDevicePixelRatio(ratio);
    m_minimapCache.fill(Qt::transparent);
    QPainter painter(&m_minimapCache);
    preparePainter(painter);
    painter.fillPath(roundedPath(QRectF(0, 0, map.width(), map.height()), kRadius), theme.gridColor);
    if (m_document && !m_document->empty()) {
        const double total = contentColumns() * m_grid.slotMs();
        const qreal rowStep = (map.height() - 6) / EditorDocument::kKeyCount;
        painter.setPen(Qt::NoPen);
        for (const EditorNote &note : m_document->notes()) {
            painter.setBrush(rowColor(note.key));
            const qreal x = static_cast<qreal>((note.time - m_grid.originMs()) / total) * map.width();
            const qreal w = std::max<qreal>(1.5, static_cast<qreal>(note.hold / total) * map.width());
            const qreal y = 3 + (EditorDocument::kKeyCount - 1 - note.key) * rowStep;
            painter.drawRect(QRectF(x, y, w, std::max<qreal>(1.5, rowStep - 0.5)));
        }
    }
    m_minimapDirty = false;
}

void TimelineView::paintMinimap(QPainter &painter)
{
    const QRectF map = minimapRect();
    if (map.width() <= 0) {
        return;
    }
    if (m_minimapDirty || m_minimapCache.isNull()) {
        rebuildMinimap();
    }
    painter.drawPixmap(map.topLeft(), m_minimapCache);
    const qreal total = contentWidth();
    if (total <= 0) {
        return;
    }
    const qreal left = map.left() + m_scroll / total * map.width();
    const qreal width = std::max<qreal>(6, rollRect().width() / total * map.width());
    const QRectF view(left, map.top() + 1, std::min(width, map.right() - left), map.height() - 2);
    painter.setPen(QPen(kAccent, 1.5));
    painter.setBrush(withAlpha(kAccent, 0.15));
    painter.drawRoundedRect(view, 4, 4);
    if (m_cursor >= 0) {
        const qreal x = map.left() + (m_cursor + 0.5) * m_columnWidth / total * map.width();
        painter.setPen(QPen(kAccent, 1));
        painter.drawLine(QPointF(x, map.top() + 2), QPointF(x, map.bottom() - 2));
    }
}

void TimelineView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_document) {
        return;
    }
    const QPointF point = event->position();
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    m_pressPoint = point;
    m_dragMoved = false;
    if (minimapRect().contains(point)) {
        m_drag = Drag::Minimap;
        scrollMinimapTo(point.x());
        return;
    }
    if (point.x() < kGutter) {
        return;
    }
    const int column = columnAtX(point.x());
    if (rulerRect().contains(point)) {
        m_drag = Drag::Ruler;
        emit columnPressed(column, shift);
        return;
    }
    if (!rollRect().contains(point)) {
        return;
    }
    m_pressColumn = column;
    m_pressKey = keyAtY(point.y());
    bool onHold = false;
    const int hit = m_editable ? noteAt(point, onHold) : -1;
    if (hit >= 0) {
        m_pressNote = m_document->notes()[static_cast<std::size_t>(hit)];
        m_drag = onHold || shift ? Drag::Hold : Drag::Note;
        m_ghostTime = m_pressNote.time;
        m_ghostKey = m_pressNote.key;
        m_ghostHold = m_pressNote.hold;
        emit columnPressed(m_grid.columnOf(static_cast<double>(m_pressNote.time)), false);
        return;
    }
    m_drag = Drag::Cell;
    emit columnPressed(column, shift);
}

void TimelineView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF point = event->position();
    const bool free = event->modifiers().testFlag(Qt::AltModifier);
    if (m_drag == Drag::None) {
        const bool inRoll = rollRect().contains(point);
        const int column = inRoll ? columnAtX(point.x()) : -1;
        const int key = inRoll ? keyAtY(point.y()) : -1;
        if (column != m_hoverColumn || key != m_hoverKey) {
            m_hoverColumn = column;
            m_hoverKey = key;
            update();
        }
        bool onHold = false;
        const int hit = inRoll && m_editable ? noteAt(point, onHold) : -1;
        setCursor(hit < 0 ? Qt::ArrowCursor : (onHold ? Qt::SizeHorCursor : Qt::OpenHandCursor));
        return;
    }
    if (!m_dragMoved && (point - m_pressPoint).manhattanLength() < kDragThreshold) {
        return;
    }
    m_dragMoved = true;
    switch (m_drag) {
    case Drag::Minimap:
        scrollMinimapTo(point.x());
        break;
    case Drag::Ruler:
    case Drag::Cell:
        emit columnPressed(columnAtX(point.x()), true);
        break;
    case Drag::Note:
        m_ghostTime = dragTime(point, free);
        m_ghostKey = keyAtY(point.y());
        setCursor(Qt::ClosedHandCursor);
        update();
        break;
    case Drag::Hold:
        m_ghostHold = std::max<std::int64_t>(0, dragTime(point, free) - m_pressNote.time);
        update();
        break;
    case Drag::None:
        break;
    }
}

void TimelineView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const Drag drag = m_drag;
    m_drag = Drag::None;
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    switch (drag) {
    case Drag::Note:
        if (m_dragMoved) {
            if (m_ghostTime != m_pressNote.time || m_ghostKey != m_pressNote.key) {
                emit noteMoved(m_pressNote.time, m_pressNote.key, m_ghostTime, m_ghostKey);
            }
        } else {
            emit cellToggled(m_grid.columnOf(static_cast<double>(m_pressNote.time)), m_pressNote.key);
        }
        break;
    case Drag::Hold:
        if (m_dragMoved && m_ghostHold != m_pressNote.hold) {
            emit holdChanged(m_pressNote.time, m_pressNote.key, m_ghostHold);
        }
        break;
    case Drag::Cell:
        if (!m_dragMoved && !shift && m_editable) {
            emit cellToggled(m_pressColumn, m_pressKey);
        }
        break;
    default:
        break;
    }
    setCursor(Qt::ArrowCursor);
    update();
}

void TimelineView::wheelEvent(QWheelEvent *event)
{
    const QPoint delta = event->angleDelta();
    const int steps = (std::abs(delta.y()) >= std::abs(delta.x()) ? delta.y() : delta.x()) / 120;
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        if (steps != 0) {
            zoomBy(steps);
        }
        event->accept();
        return;
    }
    const qreal amount = (std::abs(delta.y()) >= std::abs(delta.x()) ? delta.y() : delta.x()) / 120.0;
    setScroll(m_scroll - amount * kScrollColumnsPerNotch * m_columnWidth);
    event->accept();
}

void TimelineView::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hoverColumn = -1;
    m_hoverKey = -1;
    update();
}

}  // namespace sky
