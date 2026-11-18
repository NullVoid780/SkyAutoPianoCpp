#pragma once

#include <QImage>
#include <QPixmap>
#include <QPointer>
#include <QRect>

#include <memory>

class QWidget;

namespace sky {

class BackdropBlur {
public:
    enum class Extent { Owner, WindowWidth };

    BackdropBlur(QWidget *owner, qreal radius, Extent extent = Extent::Owner);

    void invalidate();
    void capture();
    QPixmap pixmap();

    static bool isCapturing() { return s_capturing > 0; }

private:
    QRect targetRect() const;
    void scheduleCapture();

    QPointer<QWidget> m_owner;
    qreal m_radius;
    Extent m_extent;
    QImage m_blurred;
    QRect m_source;
    qreal m_dpr = 1.0;
    QPixmap m_cache;
    QRect m_cacheRect;
    bool m_valid = false;
    bool m_scheduled = false;
    std::shared_ptr<int> m_lifetime = std::make_shared<int>(0);

    static int s_capturing;
};

}  // namespace sky
