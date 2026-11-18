#pragma once

#include <QPainter>
#include <QRectF>

#include "ui/theme.hpp"

namespace sky {

class ShadowPainter {
public:
    static void paint(QPainter &painter, const QRectF &rect, qreal cornerRadius, const ShadowSpec &shadow);
    static int extent(const ShadowSpec &shadow);
};

}  // namespace sky
