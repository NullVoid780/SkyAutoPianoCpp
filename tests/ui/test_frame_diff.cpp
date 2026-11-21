#include <catch2/catch_test_macros.hpp>

#include "ui/render/frame_diff.hpp"

using namespace sky;

namespace {

QImage blank(int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(qRgb(20, 30, 40));
    return image;
}

}  // namespace

TEST_CASE("identical frames have no changed area")
{
    CHECK(changedArea(blank(100, 80), blank(100, 80)).isEmpty());
}

TEST_CASE("a changed pixel marks its block")
{
    QImage after = blank(100, 80);
    after.setPixel(40, 50, qRgb(255, 0, 0));
    CHECK(changedArea(blank(100, 80), after) == QRect(32, 48, 16, 16));
}

TEST_CASE("changes far apart are covered by one area that stays inside the frame")
{
    QImage after = blank(100, 80);
    after.setPixel(3, 2, qRgb(255, 0, 0));
    after.setPixel(99, 79, qRgb(0, 255, 0));
    CHECK(changedArea(blank(100, 80), after) == QRect(0, 0, 100, 80));
}

TEST_CASE("a frame of a different size is a whole new frame")
{
    CHECK(changedArea(blank(100, 80), blank(120, 80)) == QRect(0, 0, 120, 80));
}
