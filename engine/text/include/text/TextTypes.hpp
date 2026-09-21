#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Engine::Text {

// A synchronous request for one UTF-8 text run. Layout, color, anchoring, and
// interaction remain caller policy; the rasterizer only produces glyph pixels.
struct TextRasterRequest final {
    std::string_view utf8;
    float pointSize{};
};

// Owning, backend-neutral straight-alpha RGBA8 pixels. rowPitch describes the
// byte distance between rows and allows direct construction of Render::ImageView.
struct TextBitmap final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t rowPitch{};
    std::vector<std::byte> rgba8;
};

} // namespace Engine::Text
