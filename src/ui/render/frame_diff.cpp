#include "ui/render/frame_diff.hpp"

#include <QImageWriter>

#include <algorithm>
#include <cstring>

namespace sky {

QRect changedArea(const QImage &before, const QImage &after)
{
    if (before.size() != after.size() || before.format() != after.format() || after.depth() != 32) {
        return after.rect();
    }
    const int width = after.width();
    const int height = after.height();
    const auto rowBytes = static_cast<std::size_t>(width) * 4;
    int top = -1;
    int bottom = -1;
    for (int y = 0; y < height; ++y) {
        if (std::memcmp(before.constScanLine(y), after.constScanLine(y), rowBytes) != 0) {
            if (top < 0) {
                top = y;
            }
            bottom = y;
        }
    }
    if (top < 0) {
        return {};
    }
    int left = width;
    int right = -1;
    for (int y = top; y <= bottom; ++y) {
        const auto *a = reinterpret_cast<const QRgb *>(before.constScanLine(y));
        const auto *b = reinterpret_cast<const QRgb *>(after.constScanLine(y));
        for (int x = 0; x < left; ++x) {
            if (a[x] != b[x]) {
                left = x;
                break;
            }
        }
        for (int x = width - 1; x > right; --x) {
            if (a[x] != b[x]) {
                right = x;
                break;
            }
        }
    }
    const int x0 = left / kFrameBlock * kFrameBlock;
    const int y0 = top / kFrameBlock * kFrameBlock;
    const int x1 = std::min(width, (right / kFrameBlock + 1) * kFrameBlock);
    const int y1 = std::min(height, (bottom / kFrameBlock + 1) * kFrameBlock);
    return QRect(x0, y0, x1 - x0, y1 - y0);
}

bool writeFrameJpeg(const QImage &image, const QString &path)
{
    QImageWriter writer(path, "jpg");
    writer.setQuality(kFrameQuality);
    writer.setOptimizedWrite(true);
    writer.setProgressiveScanWrite(true);
    return writer.write(image);
}

}  // namespace sky
