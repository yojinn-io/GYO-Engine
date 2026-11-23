#include "engine/image/ImageData.hpp"

#include <utility>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Image {

ImageData::ImageData(const std::uint32_t width, const std::uint32_t height, const ColorSpace colorSpace,
                     std::vector<std::uint8_t> pixels)
    : width_(width), height_(height), colorSpace_(colorSpace), pixels_(std::move(pixels)) {
    GYO_ASSERT(pixels_.size() == std::size_t{width} * height * kBytesPerPixel);
}

} // namespace GYO::Engine::Image
