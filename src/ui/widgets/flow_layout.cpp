#include "ui/widgets/flow_layout.hpp"

#include <QWidget>

#include <algorithm>

namespace sky {

FlowLayout::FlowLayout(QWidget *parent, int spacing, int lineSpacing)
    : QLayout(parent), m_spacing(spacing), m_lineSpacing(lineSpacing)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
    for (const Entry &entry : m_entries) {
        delete entry.item;
    }
}

void FlowLayout::addWidget(QWidget *widget, int gapBefore)
{
    addChildWidget(widget);
    m_entries.push_back(Entry{new QWidgetItem(widget), gapBefore});
    invalidate();
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_entries.push_back(Entry{item, 0});
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return index >= 0 && index < count() ? m_entries[static_cast<std::size_t>(index)].item : nullptr;
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    if (index < 0 || index >= count()) {
        return nullptr;
    }
    QLayoutItem *item = m_entries[static_cast<std::size_t>(index)].item;
    m_entries.erase(m_entries.begin() + index);
    return item;
}

int FlowLayout::heightForWidth(int width) const
{
    return arrange(QRect(0, 0, width, 0), false);
}

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const Entry &entry : m_entries) {
        if (!entry.item->isEmpty()) {
            size = size.expandedTo(entry.item->minimumSize());
        }
    }
    const QMargins margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize FlowLayout::sizeHint() const
{
    int width = 0;
    int height = 0;
    for (const Entry &entry : m_entries) {
        if (entry.item->isEmpty()) {
            continue;
        }
        const QSize hint = entry.item->sizeHint();
        width += (width > 0 ? m_spacing + entry.gapBefore : 0) + hint.width();
        height = std::max(height, hint.height());
    }
    const QMargins margins = contentsMargins();
    return QSize(width + margins.left() + margins.right(), height + margins.top() + margins.bottom());
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

int FlowLayout::arrange(const QRect &rect, bool apply) const
{
    const QRect area = rect.marginsRemoved(contentsMargins());
    int x = area.x();
    int y = area.y();
    int lineHeight = 0;
    std::vector<std::pair<QLayoutItem *, QRect>> line;
    const auto flush = [&] {
        for (auto &[item, geometry] : line) {
            geometry.moveTop(geometry.top() + (lineHeight - geometry.height()) / 2);
            if (apply) {
                item->setGeometry(geometry);
            }
        }
        line.clear();
    };
    for (const Entry &entry : m_entries) {
        if (entry.item->isEmpty()) {
            continue;
        }
        const QSize hint = entry.item->sizeHint();
        const int gap = line.empty() ? 0 : m_spacing + entry.gapBefore;
        if (!line.empty() && x + gap + hint.width() > area.right() + 1) {
            flush();
            x = area.x();
            y += lineHeight + m_lineSpacing;
            lineHeight = 0;
        } else {
            x += gap;
        }
        line.emplace_back(entry.item, QRect(QPoint(x, y), hint));
        x += hint.width();
        lineHeight = std::max(lineHeight, hint.height());
    }
    flush();
    const QMargins margins = contentsMargins();
    return y + lineHeight - rect.y() + margins.bottom();
}

}  // namespace sky
