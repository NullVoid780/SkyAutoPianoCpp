#include "ui/render/backdrop_blur.hpp"

#include <QTimer>
#include <QWidget>

#include <cmath>

#include "ui/render/box_blur.hpp"

namespace sky {

int BackdropBlur::s_capturing = 0;

BackdropBlur::BackdropBlur(QWidget *owner, qreal radius, Extent extent)
    : m_owner(owner), m_radius(radius), m_extent(extent)
{
}

QRect BackdropBlur::targetRect() const
{
    QWidget *window = m_owner->window();
    return QRect(m_owner->mapTo(window, QPoint(0, 0)), m_owner->size());
}

void BackdropBlur::invalidate()
{
    m_valid = false;
    scheduleCapture();
}

void BackdropBlur::scheduleCapture()
{
    if (m_scheduled || !m_owner) {
        return;
    }
    m_scheduled = true;
    const std::weak_ptr<int> alive = m_lifetime;
    QTimer::singleShot(0, m_owner, [this, alive] {
        if (alive.expired()) {
            return;
        }
        m_scheduled = false;
        capture();
        if (m_owner) {
            m_owner->update();
        }
    });
}

void BackdropBlur::capture()
{
    if (!m_owner || m_radius <= 0 || s_capturing > 0) {
        return;
    }
    QWidget *window = m_owner->window();
    const QRect target = targetRect();
    const int pad = static_cast<int>(std::ceil(m_radius * 2));
    QRect area = target.adjusted(-pad, -pad, pad, pad);
    if (m_extent == Extent::WindowWidth) {
        area.setLeft(0);
        area.setRight(window->width() - 1);
    }
    const QRect source = area.intersected(window->rect());
    if (source.isEmpty()) {
        return;
    }
    const qreal dpr = window->devicePixelRatioF();
    QImage image(source.size() * dpr, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    ++s_capturing;
    window->render(&image, QPoint(0, 0), QRegion(source), QWidget::DrawWindowBackground | QWidget::DrawChildren);
    --s_capturing;
    m_blurred = boxBlur(image, m_radius * dpr);
    m_source = source;
    m_dpr = dpr;
    m_cache = QPixmap();
    m_valid = true;
}

QPixmap BackdropBlur::pixmap()
{
    if (!m_owner || m_radius <= 0) {
        return {};
    }
    const QRect target = targetRect();
    const QRect visible = target.intersected(m_owner->window()->rect());
    if (!visible.isEmpty() && (!m_valid || !m_source.contains(visible))) {
        scheduleCapture();
    }
    if (m_blurred.isNull()) {
        return {};
    }
    if (!m_cache.isNull() && m_cacheRect == target) {
        return m_cache;
    }
    const QRect crop(QPoint(static_cast<int>((target.left() - m_source.left()) * m_dpr),
                            static_cast<int>((target.top() - m_source.top()) * m_dpr)),
                     target.size() * m_dpr);
    m_cache = QPixmap::fromImage(m_blurred.copy(crop));
    m_cache.setDevicePixelRatio(m_dpr);
    m_cacheRect = target;
    return m_cache;
}

}  // namespace sky
