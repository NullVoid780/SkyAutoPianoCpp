#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace sky {

class IconCache {
public:
    static QPixmap pixmap(const QString &id, int size, const QColor &color, qreal devicePixelRatio = 1.0);
    static QPixmap pixmap(const QString &id, const QSizeF &size, const QColor &color, qreal devicePixelRatio = 1.0);
    static QIcon icon(const QString &id, int size, const QColor &color);
    static bool exists(const QString &id);
};

}  // namespace sky
