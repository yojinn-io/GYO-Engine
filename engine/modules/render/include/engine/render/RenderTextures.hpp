#pragma once

// Textures made from CPU pixels, as data (docs/architecture/target/v2/render.zh-Hant.md, section 3; SDL declaration
// section 10.2, R2a: "CPU 像素的貼圖：建立與局部更新、id 誰配"). The data side gives the ids: RenderTextures hands out
// a TextureId when a texture is asked for and keeps the asks of this frame (make, update a region, let go) in order;
// a backend reads them, makes its own GPU objects and keys them by the id. Draws name a texture by its id
// (Draw2D::texture). The users are Dear ImGui's font atlas (batch K), the glyph atlas (batch U) and pictures.
//
// Pixels are in image's layout (ImageData, image.zh-Hant.md section 2): 4 bytes R, G, B, A per pixel, alpha straight,
// row 0 on top, rows with no padding.

#include <cstdint>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "engine/kernel/Hash.hpp"

namespace GYO::Engine::Render {

// A texture's id. TextureId{} (0) is the white texture, always there, never made or let go: a draw with it shows its
// vertex colours alone.
struct TextureId final {
    std::uint32_t value = 0;

    friend bool operator==(const TextureId&, const TextureId&) = default;
};

// How a texture is sampled between its pixels.
enum class TextureFilter : std::uint8_t {
    Linear = 1,
    // The nearest pixel (pixel art; text drawn at its own size).
    Nearest,
};

struct TextureDesc final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    TextureFilter filter = TextureFilter::Linear;

    friend bool operator==(const TextureDesc&, const TextureDesc&) = default;
};

// A rectangle of a texture's pixels.
struct TextureRegion final {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    friend bool operator==(const TextureRegion&, const TextureRegion&) = default;
};

struct TextureCreate final {
    TextureId id;
    TextureDesc desc;
    // width * height * 4 bytes.
    std::vector<std::uint8_t> pixels;
};

struct TextureUpdate final {
    TextureId id;
    TextureRegion region;
    // region.width * region.height * 4 bytes, the region's rows only.
    std::vector<std::uint8_t> pixels;
};

struct TextureRelease final {
    TextureId id;
};

using TextureOp = std::variant<TextureCreate, TextureUpdate, TextureRelease>;

// render_world's Resource. The asks of a frame stay until render_world clears them after the frame was drawn (its
// Cleanup stage); a backend reads them in order before it draws. Ids are never given twice.
class RenderTextures final {
public:
    // A new texture with these pixels. A zero width or height, or pixels of another size, is a Programmer Error.
    TextureId Create(TextureDesc desc, std::vector<std::uint8_t> pixels);
    // Replaces a region of a texture. An id that is not live (never made, or let go), a region not inside the
    // texture or of zero size, or pixels of another size, is a Programmer Error.
    void Update(TextureId id, TextureRegion region, std::vector<std::uint8_t> pixels);
    // Lets the texture go; draws must not name it any more. An id that is not live is a Programmer Error.
    void Release(TextureId id);

    // The live texture's description, or nullptr.
    [[nodiscard]] const TextureDesc* Find(TextureId id) const noexcept;
    [[nodiscard]] std::size_t LiveCount() const noexcept { return live_.size(); }
    // This frame's asks, in the order they were made.
    [[nodiscard]] std::span<const TextureOp> Pending() const noexcept { return pending_; }
    // render_world, after each frame was drawn (Cleanup).
    void ClearPending() noexcept { pending_.clear(); }

private:
    std::uint32_t next_ = 1;
    std::vector<std::pair<std::uint32_t, TextureDesc>> live_;
    std::vector<TextureOp> pending_;
};

inline void HashValue(Kernel::Hasher&, const RenderTextures&) {}

} // namespace GYO::Engine::Render
