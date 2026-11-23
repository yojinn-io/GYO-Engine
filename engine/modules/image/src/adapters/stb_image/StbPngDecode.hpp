#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// The one adapter of stb_image (Design Invariant IM-1): the rest of the Module
// sees only this function, in its own words.

namespace GYO::Engine::Image::Detail {

struct DecodedRgba8 final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // width * height * 4 bytes, R G B A, row 0 the top row.
    std::vector<std::uint8_t> pixels;
};

// Decodes a whole PNG file to 8-bit RGBA, top row first, straight alpha.
// Nothing when the decoder refuses the bytes. Keeps no state between calls
// and may run on many threads at once.
[[nodiscard]] std::optional<DecodedRgba8> DecodePngToRgba8(std::span<const std::byte> bytes);

} // namespace GYO::Engine::Image::Detail
