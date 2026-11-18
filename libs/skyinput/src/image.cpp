#include "skyinput/image.hpp"

#include <utility>

namespace skyinput {

namespace {

double luminance(const std::vector<std::uint8_t> &pixels, std::size_t i)
{
    return pixels[i] * 0.114 + pixels[i + 1] * 0.587 + pixels[i + 2] * 0.299;
}

}  // namespace

void convertBgraPixels(std::vector<std::uint8_t> &pixels, PixelFormat format, std::uint8_t threshold)
{
    const std::size_t size = pixels.size() - pixels.size() % 4;
    switch (format) {
    case PixelFormat::Rgba:
        for (std::size_t i = 0; i < size; i += 4) {
            std::swap(pixels[i], pixels[i + 2]);
            pixels[i + 3] = 255;
        }
        break;
    case PixelFormat::Bgra:
        break;
    case PixelFormat::Grey: {
        std::size_t j = 0;
        for (std::size_t i = 0; i < size; i += 4, ++j) {
            pixels[j] = static_cast<std::uint8_t>(luminance(pixels, i));
        }
        pixels.resize(j);
        break;
    }
    case PixelFormat::Monochrome: {
        std::size_t j = 0;
        for (std::size_t i = 0; i < size; i += 4, ++j) {
            pixels[j] = luminance(pixels, i) < threshold ? 0 : 255;
        }
        pixels.resize(j);
        break;
    }
    }
}

}  // namespace skyinput
