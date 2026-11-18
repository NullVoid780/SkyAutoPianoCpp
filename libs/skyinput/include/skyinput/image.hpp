#pragma once

#include <cstdint>
#include <vector>

namespace skyinput {

enum class PixelFormat { Rgba, Bgra, Grey, Monochrome };

struct Image {
    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::Rgba;
    std::vector<std::uint8_t> data;
};

void convertBgraPixels(std::vector<std::uint8_t> &pixels, PixelFormat format, std::uint8_t threshold);

}  // namespace skyinput
