#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <QImage>
#include <QPainter>
#include <QRandomGenerator>

#include "ui/practice/sky_calibrator.hpp"

using namespace sky;
using Catch::Matchers::WithinAbs;

namespace {

QImage scene(quint32 seed)
{
    QImage image(480, 270, QImage::Format_Grayscale8);
    QRandomGenerator random(seed);
    for (int y = 0; y < image.height(); ++y) {
        uchar *line = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            line[x] = static_cast<uchar>(90 + random.bounded(18));
        }
    }
    return image;
}

}  // namespace

TEST_CASE("a key that lights up is found at its centre despite small changes elsewhere")
{
    const QImage before = scene(1);
    QImage after = scene(2);
    {
        QPainter painter(&after);
        painter.fillRect(QRect(130, 60, 14, 14), QColor(235, 235, 235));
    }
    const auto point = locateFlash(before, after, QRectF(0, 0, 1, 1));
    REQUIRE(point);
    CHECK_THAT(point->x(), WithinAbs(137, 1.5));
    CHECK_THAT(point->y(), WithinAbs(67, 1.5));
}

TEST_CASE("no flash is reported when nothing lit up")
{
    CHECK_FALSE(locateFlash(scene(3), scene(4), QRectF(0, 0, 1, 1)));
}

TEST_CASE("a flash outside the search area is ignored")
{
    const QImage before = scene(5);
    QImage after = before.copy();
    {
        QPainter painter(&after);
        painter.fillRect(QRect(400, 200, 14, 14), Qt::white);
    }
    CHECK_FALSE(locateFlash(before, after, QRectF(0, 0, 0.5, 0.5)));
    CHECK(locateFlash(before, after, QRectF(0.5, 0.5, 0.5, 0.5)));
}

TEST_CASE("the grid is rebuilt from Sky's top-left and bottom-right keys")
{
    const auto grid = gridFromCorners(QPointF(384, 128), QPointF(896, 414), QSizeF(1280, 720));
    REQUIRE(grid);
    CHECK_THAT(grid->centerX, WithinAbs(0.5, 0.001));
    CHECK_THAT(grid->centerY, WithinAbs(0.376, 0.002));
    CHECK_THAT(grid->scaleX, WithinAbs(1.0, 0.001));
    CHECK_THAT(grid->scaleY, WithinAbs(1.0, 0.01));
}

TEST_CASE("a wider screen gets its own horizontal spacing")
{
    const auto grid = gridFromCorners(QPointF(1060, 160), QPointF(1700, 520), QSizeF(2560, 1080));
    REQUIRE(grid);
    CHECK_THAT(grid->scaleX, WithinAbs(160.0 / 256.0, 0.001));
    CHECK_THAT(grid->scaleY, WithinAbs(180.0 / (1080 * 0.199), 0.001));
}

TEST_CASE("corners in the wrong order or a squashed grid are rejected")
{
    CHECK_FALSE(gridFromCorners(QPointF(896, 414), QPointF(384, 128), QSizeF(1280, 720)));
    CHECK_FALSE(gridFromCorners(QPointF(384, 128), QPointF(896, 140), QSizeF(1280, 720)));
}
