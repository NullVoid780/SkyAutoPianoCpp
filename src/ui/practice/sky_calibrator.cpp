#include "ui/practice/sky_calibrator.hpp"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <vector>

#include "model/key_table.hpp"

namespace sky {

namespace {

constexpr int kWorkWidth = 480;
constexpr int kNoise = 22;
constexpr double kBoxShare = 0.026;
constexpr double kMinAverage = 10.0;
constexpr int kPollMs = 90;
constexpr int kTimeoutMs = 25000;
constexpr int kShotDelays[] = {70, 150, 260};
constexpr double kColumnShare = 0.10;
constexpr double kRowShare = 0.199;
constexpr double kMinScale = 0.35;
constexpr double kMaxScale = 2.5;
constexpr double kMaxSkew = 1.8;
const QRectF kTopLeftArea(0.08, 0.02, 0.45, 0.45);
const QRectF kBottomRightArea(0.45, 0.25, 0.47, 0.5);

QImage workImage(const QImage &image)
{
    if (image.isNull()) {
        return {};
    }
    return image.scaledToWidth(kWorkWidth, Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
}

QImage brighter(const QImage &a, const QImage &b)
{
    if (a.isNull()) {
        return b;
    }
    QImage result = a.copy();
    for (int y = 0; y < result.height(); ++y) {
        uchar *out = result.scanLine(y);
        const uchar *other = b.constScanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            out[x] = std::max(out[x], other[x]);
        }
    }
    return result;
}

}  // namespace

std::optional<QPointF> locateFlash(const QImage &beforeImage, const QImage &afterImage, const QRectF &searchArea)
{
    const QImage before = beforeImage.convertToFormat(QImage::Format_Grayscale8);
    const QImage after = afterImage.convertToFormat(QImage::Format_Grayscale8);
    if (before.isNull() || before.size() != after.size()) {
        return std::nullopt;
    }
    const int width = before.width();
    const int height = before.height();
    const int left = std::clamp(static_cast<int>(searchArea.left() * width), 0, width - 1);
    const int top = std::clamp(static_cast<int>(searchArea.top() * height), 0, height - 1);
    const int right = std::clamp(static_cast<int>(searchArea.right() * width), left + 1, width);
    const int bottom = std::clamp(static_cast<int>(searchArea.bottom() * height), top + 1, height);

    std::vector<int> response(static_cast<std::size_t>(width * height), 0);
    for (int y = top; y < bottom; ++y) {
        const uchar *b = before.constScanLine(y);
        const uchar *a = after.constScanLine(y);
        for (int x = left; x < right; ++x) {
            response[static_cast<std::size_t>(y * width + x)] = std::max(0, a[x] - b[x] - kNoise);
        }
    }
    std::vector<long long> integral(static_cast<std::size_t>((width + 1) * (height + 1)), 0);
    for (int y = 0; y < height; ++y) {
        long long row = 0;
        for (int x = 0; x < width; ++x) {
            row += response[static_cast<std::size_t>(y * width + x)];
            integral[static_cast<std::size_t>((y + 1) * (width + 1) + x + 1)] =
                integral[static_cast<std::size_t>(y * (width + 1) + x + 1)] + row;
        }
    }
    const auto boxSum = [&](int x0, int y0, int x1, int y1) {
        x0 = std::clamp(x0, 0, width);
        x1 = std::clamp(x1, 0, width);
        y0 = std::clamp(y0, 0, height);
        y1 = std::clamp(y1, 0, height);
        return integral[static_cast<std::size_t>(y1 * (width + 1) + x1)] -
               integral[static_cast<std::size_t>(y0 * (width + 1) + x1)] -
               integral[static_cast<std::size_t>(y1 * (width + 1) + x0)] +
               integral[static_cast<std::size_t>(y0 * (width + 1) + x0)];
    };
    const int box = std::max(3, static_cast<int>(width * kBoxShare));
    long long best = 0;
    int bestX = -1;
    int bestY = -1;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const long long sum = boxSum(x - box / 2, y - box / 2, x + box / 2 + 1, y + box / 2 + 1);
            if (sum > best) {
                best = sum;
                bestX = x;
                bestY = y;
            }
        }
    }
    if (bestX < 0 || static_cast<double>(best) / (box * box) < kMinAverage) {
        return std::nullopt;
    }
    double weight = 0;
    double sumX = 0;
    double sumY = 0;
    for (int y = std::max(top, bestY - box); y < std::min(bottom, bestY + box + 1); ++y) {
        for (int x = std::max(left, bestX - box); x < std::min(right, bestX + box + 1); ++x) {
            const int value = response[static_cast<std::size_t>(y * width + x)];
            weight += value;
            sumX += value * (x + 0.5);
            sumY += value * (y + 0.5);
        }
    }
    if (weight <= 0) {
        return std::nullopt;
    }
    return QPointF(sumX / weight, sumY / weight);
}

std::optional<SkyGrid> gridFromCorners(const QPointF &topLeftKey, const QPointF &bottomRightKey, const QSizeF &window)
{
    if (window.width() <= 0 || window.height() <= 0) {
        return std::nullopt;
    }
    const double columnStep = (bottomRightKey.x() - topLeftKey.x()) / 4.0;
    const double rowStep = (bottomRightKey.y() - topLeftKey.y()) / 2.0;
    if (columnStep <= 0 || rowStep <= 0) {
        return std::nullopt;
    }
    SkyGrid grid;
    grid.centerX = (topLeftKey.x() + bottomRightKey.x()) / 2.0 / window.width();
    grid.centerY = (topLeftKey.y() + bottomRightKey.y()) / 2.0 / window.height();
    grid.scaleX = columnStep / (window.width() * kColumnShare);
    grid.scaleY = rowStep / (window.height() * kRowShare);
    const bool sane = grid.scaleX >= kMinScale && grid.scaleX <= kMaxScale && grid.scaleY >= kMinScale &&
                      grid.scaleY <= kMaxScale && grid.scaleX / grid.scaleY <= kMaxSkew &&
                      grid.scaleY / grid.scaleX <= kMaxSkew;
    if (!sane) {
        return std::nullopt;
    }
    return grid;
}

SkyCalibrator::SkyCalibrator(QObject *parent) : QObject(parent), m_poll(new QTimer(this)), m_timeout(new QTimer(this))
{
    m_poll->setInterval(kPollMs);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(kTimeoutMs);
    connect(m_poll, &QTimer::timeout, this, [this] {
        if (m_step == Step::TopLeft || m_step == Step::BottomRight) {
            m_latest = workImage(capture());
        }
    });
    connect(m_timeout, &QTimer::timeout, this,
            [this] { fail(QStringLiteral("Auto-align timed out. Open Sky's instrument and try again.")); });
}

QImage SkyCalibrator::capture() const
{
    QScreen *screen = QGuiApplication::screenAt(m_rect.center());
    if (!screen) {
        return {};
    }
    const QPoint origin = m_rect.topLeft() - screen->geometry().topLeft();
    return screen->grabWindow(0, origin.x(), origin.y(), m_rect.width(), m_rect.height()).toImage();
}

void SkyCalibrator::start(const QRect &skyRect)
{
    m_rect = skyRect;
    m_topLeft.reset();
    m_latest = workImage(capture());
    if (m_latest.isNull()) {
        emit finished(false, SkyGrid{}, QStringLiteral("Couldn't take a picture of the Sky window."));
        return;
    }
    m_step = Step::TopLeft;
    m_poll->start();
    m_timeout->start();
    emit instruction(QStringLiteral("Press Y in Sky"));
    emit prompt(1, QStringLiteral("Y"), QStringLiteral("The top-left key. Keep Sky's instrument open."));
}

void SkyCalibrator::cancel()
{
    if (m_step == Step::Idle) {
        return;
    }
    m_step = Step::Idle;
    m_poll->stop();
    m_timeout->stop();
    emit finished(false, SkyGrid{}, QStringLiteral("Auto-align cancelled."));
}

void SkyCalibrator::fail(const QString &message)
{
    m_step = Step::Idle;
    m_poll->stop();
    m_timeout->stop();
    emit finished(false, SkyGrid{}, message);
}

void SkyCalibrator::keyPressed(int key)
{
    const bool expected = (m_step == Step::TopLeft && key == 0) || (m_step == Step::BottomRight && key == kSkyKeyCount - 1);
    if (!expected) {
        return;
    }
    m_before = m_latest;
    m_measuring = m_step;
    m_step = Step::Measuring;
    m_best = QImage();
    m_shots = 0;
    for (const int delay : kShotDelays) {
        QTimer::singleShot(delay, this, [this] {
            if (m_step != Step::Measuring) {
                return;
            }
            m_best = brighter(m_best, workImage(capture()));
            if (++m_shots == static_cast<int>(std::size(kShotDelays))) {
                measure();
            }
        });
    }
}

void SkyCalibrator::measure()
{
    const bool first = m_measuring == Step::TopLeft;
    const auto point = locateFlash(m_before, m_best, first ? kTopLeftArea : kBottomRightArea);
    if (!point) {
        m_step = m_measuring;
        m_latest = workImage(capture());
        emit instruction(first ? QStringLiteral("Didn't see it, press Y again") : QStringLiteral("Didn't see it, press / again"));
        emit prompt(first ? 1 : 2, first ? QStringLiteral("Y") : QStringLiteral("/"),
                    QStringLiteral("Didn't see the key light up. Press it again."));
        return;
    }
    const double factor = static_cast<double>(m_rect.width()) / std::max(1, m_best.width());
    const QPointF logical(point->x() * factor, point->y() * factor);
    if (first) {
        m_topLeft = logical;
        m_step = Step::BottomRight;
        m_latest = workImage(capture());
        emit instruction(QStringLiteral("Now press / in Sky"));
        emit prompt(2, QStringLiteral("/"), QStringLiteral("The bottom-right key."));
        return;
    }
    m_step = Step::Idle;
    m_poll->stop();
    m_timeout->stop();
    const auto grid = gridFromCorners(*m_topLeft, logical, QSizeF(m_rect.size()));
    if (!grid) {
        emit finished(false, SkyGrid{}, QStringLiteral("Those keys didn't look right. Open the instrument and try again."));
        return;
    }
    emit finished(true, *grid, QStringLiteral("Lined up with Sky's keys"));
}

}  // namespace sky
