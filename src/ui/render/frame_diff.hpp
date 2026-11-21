#pragma once

#include <QImage>
#include <QRect>
#include <QString>

namespace sky {

inline constexpr int kFrameBlock = 16;
inline constexpr int kFrameQuality = 82;

QRect changedArea(const QImage &before, const QImage &after);
bool writeFrameJpeg(const QImage &image, const QString &path);

}  // namespace sky
