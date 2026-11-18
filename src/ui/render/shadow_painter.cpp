#include "ui/render/shadow_painter.hpp"

#include <QCache>
#include <QPainterPath>
#include <QPixmap>

#include <cmath>

#include "ui/render/box_blur.hpp"

namespace sky {

namespace {

QCache<QString, QPixmap> &cache()
{
    static QCache<QString, QPixmap> pixmaps(64 * 1024 * 1024);
    return pixmaps;
}

QPixmap renderShadow(const QSizeF &shape, int pad, qreal cornerRadius, const ShadowSpec &shadow, qreal dpr)
{
    const QSizeF logical(shape.width() + pad * 2, shape.height() + pad * 2);
    QImage image((logical * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(dpr, dpr);
        QPainterPath path;
        path.addRoundedRect(QRectF(QPointF(pad, pad), shape), cornerRadius, cornerRadius);
        painter.fillPath(path, shadow.color);
    }
    QPixmap pixmap = QPixmap::fromImage(boxBlur(image, shadow.blur * dpr));
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

QPixmap cachedShadow(const QString &key, const QSizeF &shape, int pad, qreal cornerRadius, const ShadowSpec &shadow,
                     qreal dpr)
{
    if (QPixmap *hit = cache().object(key)) {
        return *hit;
    }
    QPixmap pixmap = renderShadow(shape, pad, cornerRadius, shadow, dpr);
    const qsizetype cost = static_cast<qsizetype>(pixmap.width()) * pixmap.height() * 4;
    cache().insert(key, new QPixmap(pixmap), cost);
    return pixmap;
}

}  // namespace

int ShadowPainter::extent(const ShadowSpec &shadow)
{
    return static_cast<int>(std::ceil(shadow.blur)) + 2;
}

void ShadowPainter::paint(QPainter &painter, const QRectF &rect, qreal cornerRadius, const ShadowSpec &shadow)
{
    if (shadow.color.alpha() == 0 || rect.isEmpty()) {
        return;
    }
    const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : 1.0;
    const int pad = extent(shadow);
    const int corner = pad * 2 + static_cast<int>(std::ceil(cornerRadius));
    const QPointF origin(rect.left() - pad + shadow.offset.x(), rect.top() - pad + shadow.offset.y());
    const QString style = QStringLiteral("%1:%2:%3:%4").arg(cornerRadius).arg(shadow.blur).arg(shadow.color.rgba()).arg(dpr);

    const qreal fullWidth = rect.width() + pad * 2;
    const qreal fullHeight = rect.height() + pad * 2;
    if (fullWidth < corner * 2 + 1 || fullHeight < corner * 2 + 1) {
        const QString key = QStringLiteral("exact:%1x%2:%3").arg(rect.width()).arg(rect.height()).arg(style);
        painter.drawPixmap(origin, cachedShadow(key, rect.size(), pad, cornerRadius, shadow, dpr));
        return;
    }

    const qreal coreSide = corner * 2 + 1 - pad * 2;
    const QPixmap tile =
        cachedShadow(QStringLiteral("slice:") + style, QSizeF(coreSide, coreSide), pad, cornerRadius, shadow, dpr);
    const qreal tileSide = coreSide + pad * 2;

    const qreal xs[4] = {0, static_cast<qreal>(corner), fullWidth - corner, fullWidth};
    const qreal ys[4] = {0, static_cast<qreal>(corner), fullHeight - corner, fullHeight};
    const qreal sx[4] = {0, static_cast<qreal>(corner), tileSide - corner, tileSide};

    const bool smooth = painter.testRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const QRectF target(origin.x() + xs[column], origin.y() + ys[row], xs[column + 1] - xs[column],
                                ys[row + 1] - ys[row]);
            const QRectF source(sx[column] * dpr, sx[row] * dpr, (sx[column + 1] - sx[column]) * dpr,
                                (sx[row + 1] - sx[row]) * dpr);
            painter.drawPixmap(target, tile, source);
        }
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
}

}  // namespace sky
