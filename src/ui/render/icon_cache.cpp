#include "ui/render/icon_cache.hpp"

#include <QCache>
#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QSvgRenderer>

namespace sky {

namespace {

QString resourcePath(const QString &id)
{
    return QStringLiteral(":/icons/%1.svg").arg(id);
}

QCache<QString, QPixmap> &cache()
{
    static QCache<QString, QPixmap> pixmaps(512);
    return pixmaps;
}

}  // namespace

bool IconCache::exists(const QString &id)
{
    return QFile::exists(resourcePath(id));
}

QPixmap IconCache::pixmap(const QString &id, const QSizeF &size, const QColor &color, qreal dpr)
{
    const QString key = QStringLiteral("%1|%2x%3|%4|%5").arg(id).arg(size.width()).arg(size.height()).arg(color.rgba()).arg(dpr);
    if (QPixmap *cached = cache().object(key)) {
        return *cached;
    }
    QSvgRenderer renderer(resourcePath(id));
    const QSize pixels = (size * dpr).toSize();
    QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (renderer.isValid()) {
        QPainter painter(&image);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(pixels)));
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), color);
    }
    QPixmap result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(dpr);
    cache().insert(key, new QPixmap(result));
    return result;
}

QPixmap IconCache::pixmap(const QString &id, int size, const QColor &color, qreal dpr)
{
    return pixmap(id, QSizeF(size, size), color, dpr);
}

QIcon IconCache::icon(const QString &id, int size, const QColor &color)
{
    QIcon icon;
    for (qreal dpr : {1.0, 1.25, 1.5, 2.0}) {
        icon.addPixmap(pixmap(id, size, color, dpr));
    }
    if (const auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        icon.addPixmap(pixmap(id, size, color, app->devicePixelRatio()));
    }
    return icon;
}

}  // namespace sky
