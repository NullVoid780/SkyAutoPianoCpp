#include "ui/core/elevation_layer.hpp"

#include <QEvent>
#include <QPaintEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

#include "ui/render/backdrop_blur.hpp"
#include "ui/render/paint_utils.hpp"
#include "ui/render/shadow_painter.hpp"

namespace sky {

ElevationLayer::ElevationLayer(QWidget *host) : QWidget(host), m_host(host)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    host->installEventFilter(this);
    syncGeometry();
    lower();
}

void ElevationLayer::syncGeometry()
{
    setGeometry(m_host->rect());
    if (!m_floating) {
        lower();
    }
}

void ElevationLayer::setFloating(bool floating)
{
    m_floating = floating;
    if (floating) {
        raise();
    } else {
        lower();
    }
}

QRect ElevationLayer::shadowBounds(const Entry &entry) const
{
    QWidget *widget = entry.widget;
    if (!widget || !widget->isVisible() || !entry.shadows) {
        return {};
    }
    const QRect rect = bodyRect(entry).toAlignedRect();
    QRect bounds;
    for (const ShadowSpec &shadow : entry.shadows()) {
        const int pad = ShadowPainter::extent(shadow);
        const int dx = static_cast<int>(std::ceil(std::abs(shadow.offset.x())));
        const int dy = static_cast<int>(std::ceil(std::abs(shadow.offset.y())));
        bounds |= rect.adjusted(-pad - dx, -pad - dy, pad + dx, pad + dy);
    }
    return bounds;
}

QRectF ElevationLayer::bodyRect(const Entry &entry) const
{
    const QPointF origin = entry.widget->mapTo(m_host, QPoint(0, 0));
    if (entry.body) {
        return entry.body().translated(origin);
    }
    return QRectF(origin, QSizeF(entry.widget->size()));
}

void ElevationLayer::refresh(QWidget *widget)
{
    for (Entry &entry : m_entries) {
        if (entry.widget == widget) {
            const QRect bounds = shadowBounds(entry);
            update(entry.painted | bounds);
            entry.painted = bounds;
        }
    }
}

void ElevationLayer::track(QWidget *widget, qreal cornerRadius, ShadowSource shadows, BodySource body)
{
    untrack(widget);
    m_entries.push_back(Entry{widget, cornerRadius, std::move(shadows), std::move(body), {}});
    widget->installEventFilter(this);
    refresh(widget);
}

void ElevationLayer::untrack(QWidget *widget)
{
    for (const Entry &entry : m_entries) {
        if (entry.widget == widget || !entry.widget) {
            update(entry.painted);
        }
    }
    std::erase_if(m_entries, [widget](const Entry &entry) { return entry.widget == widget || !entry.widget; });
}

bool ElevationLayer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_host) {
        if (event->type() == QEvent::Resize) {
            syncGeometry();
            for (Entry &entry : m_entries) {
                entry.painted = shadowBounds(entry);
            }
            update();
        }
        return false;
    }
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::EnabledChange:
        refresh(qobject_cast<QWidget *>(watched));
        break;
    default:
        break;
    }
    return false;
}

void ElevationLayer::paintEvent(QPaintEvent *event)
{
    if (m_floating && BackdropBlur::isCapturing()) {
        return;
    }
    QPainter painter(this);
    preparePainter(painter);
    const QRect dirty = event->rect();
    for (Entry &entry : m_entries) {
        const QRect bounds = shadowBounds(entry);
        if (bounds.isEmpty() || !bounds.intersects(dirty)) {
            continue;
        }
        const QRectF rect = bodyRect(entry);
        for (const ShadowSpec &shadow : entry.shadows()) {
            ShadowPainter::paint(painter, rect, entry.radius, shadow);
        }
    }
}

}  // namespace sky
