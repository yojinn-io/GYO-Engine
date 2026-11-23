#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "engine/asset/AssetTypeName.hpp"

// A decoded image in the one pixel layout the engine hands to rendering
// (docs/architecture/target/v2/image.zh-Hant.md, section 2; Design Invariant
// IM-2):
//
// - 4 bytes per pixel, R, G, B, A in that order, 8 bits each;
// - alpha is straight (not premultiplied);
// - row 0 is the top row of the picture and pixel 0 of a row its left
//   pixel, so texture coordinate (0, 0) is the top-left corner; nothing is
//   flipped on load, and a renderer whose quads run bottom-up turns the
//   coordinates round at its one UV conversion (master pitfall 17);
// - rows follow each other with no padding: RowPitch() is Width() * 4;
// - the colour space says how a renderer samples the bytes: Srgb (colour
//   pictures, the default) or Linear (data such as normal maps).
//
// Immutable once made; read it from any thread and destroy it on any thread
// (AS-10 (c)). Its type name for the catalog is "image".

namespace GYO::Engine::Image {

enum class ColorSpace : std::uint8_t {
    Srgb = 1,
    Linear = 2,
};

[[nodiscard]] constexpr const char* ToString(const ColorSpace space) noexcept {
    switch (space) {
    case ColorSpace::Srgb: return "Srgb";
    case ColorSpace::Linear: return "Linear";
    }
    return "Unknown";
}

class ImageData final {
public:
    static constexpr std::uint32_t kBytesPerPixel = 4;

    // 0 by 0, no pixels.
    ImageData() = default;

    // Takes the pixels in the layout above. pixels.size() other than
    // width * height * 4 is a Programmer Error.
    ImageData(std::uint32_t width, std::uint32_t height, ColorSpace colorSpace, std::vector<std::uint8_t> pixels);

    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }
    // Bytes from one row to the next.
    [[nodiscard]] std::uint32_t RowPitch() const noexcept { return width_ * kBytesPerPixel; }
    [[nodiscard]] ColorSpace GetColorSpace() const noexcept { return colorSpace_; }
    [[nodiscard]] std::span<const std::uint8_t> Pixels() const noexcept { return pixels_; }

    [[nodiscard]] friend bool operator==(const ImageData&, const ImageData&) = default;

private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    ColorSpace colorSpace_ = ColorSpace::Srgb;
    std::vector<std::uint8_t> pixels_;
};

} // namespace GYO::Engine::Image

template <>
struct GYO::Engine::Asset::AssetTypeName<GYO::Engine::Image::ImageData> {
    static constexpr std::string_view value = "image";
};
