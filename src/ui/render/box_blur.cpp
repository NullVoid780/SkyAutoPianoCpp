#include "ui/render/box_blur.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace sky {

namespace {

std::array<int, 3> boxSizesForGauss(qreal sigma)
{
    const qreal ideal = std::sqrt((12 * sigma * sigma / 3) + 1);
    int lower = static_cast<int>(std::floor(ideal));
    if (lower % 2 == 0) {
        --lower;
    }
    const int upper = lower + 2;
    const qreal m = (12 * sigma * sigma - 3 * lower * lower - 12 * lower - 9) / (-4 * lower - 4);
    const int count = static_cast<int>(std::round(m));
    std::array<int, 3> sizes{};
    for (int i = 0; i < 3; ++i) {
        sizes[static_cast<std::size_t>(i)] = i < count ? lower : upper;
    }
    return sizes;
}

void blurLine(const QRgb *source, QRgb *target, int length, int stride, int radius)
{
    const qreal scale = 1.0 / (radius + radius + 1);
    int sum[4] = {};
    const auto channels = [](QRgb px, int out[4]) {
        out[0] = qAlpha(px);
        out[1] = qRed(px);
        out[2] = qGreen(px);
        out[3] = qBlue(px);
    };
    int first[4];
    int last[4];
    channels(source[0], first);
    channels(source[(length - 1) * stride], last);
    for (int c = 0; c < 4; ++c) {
        sum[c] = (radius + 1) * first[c];
    }
    for (int i = 0; i < radius; ++i) {
        int px[4];
        channels(source[std::min(i, length - 1) * stride], px);
        for (int c = 0; c < 4; ++c) {
            sum[c] += px[c];
        }
    }
    for (int i = 0; i < length; ++i) {
        int incoming[4];
        int outgoing[4];
        channels(source[std::min(i + radius, length - 1) * stride], incoming);
        channels(source[std::max(i - radius - 1, 0) * stride], outgoing);
        for (int c = 0; c < 4; ++c) {
            sum[c] += incoming[c] - (i - radius - 1 >= 0 ? outgoing[c] : first[c]);
        }
        target[i * stride] = qRgba(qBound(0, qRound(sum[1] * scale), 255), qBound(0, qRound(sum[2] * scale), 255),
                                   qBound(0, qRound(sum[3] * scale), 255), qBound(0, qRound(sum[0] * scale), 255));
    }
}

void boxPass(QImage &image, QImage &scratch, int radius)
{
    const int width = image.width();
    const int height = image.height();
    for (int y = 0; y < height; ++y) {
        blurLine(reinterpret_cast<const QRgb *>(image.constScanLine(y)), reinterpret_cast<QRgb *>(scratch.scanLine(y)),
                 width, 1, radius);
    }
    const int stride = static_cast<int>(scratch.bytesPerLine() / 4);
    for (int x = 0; x < width; ++x) {
        blurLine(reinterpret_cast<const QRgb *>(scratch.constBits()) + x, reinterpret_cast<QRgb *>(image.bits()) + x,
                 height, stride, radius);
    }
}

}  // namespace

QImage boxBlur(const QImage &source, qreal radius)
{
    QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (radius <= 0 || image.isNull()) {
        return image;
    }
    QImage scratch(image.size(), image.format());
    for (int size : boxSizesForGauss(radius / 2.0)) {
        boxPass(image, scratch, (size - 1) / 2);
    }
    return image;
}

}  // namespace sky
