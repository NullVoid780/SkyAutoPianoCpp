#pragma once

#include <QImage>

namespace sky {

QImage boxBlur(const QImage &source, qreal radius);

}  // namespace sky
