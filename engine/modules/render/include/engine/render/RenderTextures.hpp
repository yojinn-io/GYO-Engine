#pragma once

// Textures, as data (docs/architecture/target/v2/render.zh-Hant.md, section 3; SDL declaration section 10.2, R2a:
// "CPU 像素的貼圖：建立與局部更新、id 誰配"; render-pipeline declaration section 13: render targets and formats). The
// data side gives the ids: RenderTextures hands out a TextureId when a texture is asked for and keeps the asks of
// this frame (make, update a region, let go, make a target) in order; a backend reads them, makes its own GPU
// objects and keys them by the id. Draws name a texture by its id (Draw2D::texture, Draw3D::texture). The users are
// Dear ImGui's font atlas (batch K), the glyph atlas (batch U), pictures and render targets.
//
// A render target is a texture a pass draws into (its id as the pass's target, Pass2D::target) and a later pass
// samples (its id as a draw's texture), as Bevy's Image made with Image::new_target_texture
// (bevy_image/src/image.rs:1279) is both a camera's RenderTarget::Image and a sprite's picture. Its pixels are what
// the passes drew: it has no CPU pixels and no updates.
//
// Pixels are in image's layout (ImageData, image.zh-Hant.md section 2): 4 bytes R, G, B, A per pixel, alpha straight,
// row 0 on top, rows with no padding.

#include <cstdint>
#include <optional>
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

// How a texture's texels are stored and read (Bevy's TextureFormat, of which these two are the ones in use; a format
// comes with the batch that needs it: Rgba16Float with HDR (A4), a depth format with depth-only passes (A2), the
// compressed ones with F1).
enum class TextureFormat : std::uint8_t {
    // R, G, B, A, 8 bits each, sampled as stored: the 2D pass's pictures (it blends bytes, RP-4), data pictures (the
    // normal map _N, _ORM), a target whose bytes are blended as they are.
    Rgba8Unorm = 1,
    // The same bytes, R, G and B sRGB-encoded: sampling decodes them to linear (the colour pictures _D, _E that 3D
    // lights in linear space), drawing encodes linear values (alpha is linear in both). Bevy's default for colour
    // pictures (ImageLoaderSettings::is_srgb, bevy_image/src/image_loader.rs:148,176).
    Rgba8UnormSrgb,
};

[[nodiscard]] constexpr const char* ToString(const TextureFormat format) noexcept {
    switch (format) {
    case TextureFormat::Rgba8Unorm: return "Rgba8Unorm";
    case TextureFormat::Rgba8UnormSrgb: return "Rgba8UnormSrgb";
    }
    return "Unknown";
}

struct TextureDesc final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    TextureFilter filter = TextureFilter::Linear;
    // Rgba8Unorm by default: the bytes are sampled as stored, as every texture was before formats (the 2D pass and the
    // unlit 3D pass blend bytes). A picture one draw needs decoded and another as stored is two textures: SDL_GPU 3.4
    // makes each texture with one format and has no view of another format (render-pipeline declaration 13.1).
    TextureFormat format = TextureFormat::Rgba8Unorm;

    friend bool operator==(const TextureDesc&, const TextureDesc&) = default;
};

// A target's size in pixels.
struct TargetSize final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    friend bool operator==(const TargetSize&, const TargetSize&) = default;
};

// A render target: a texture passes draw into and later draws sample.
struct RenderTargetDesc final {
    // Its size; nullopt: the size of the main target (target 0: the window, or the picture a backend captures), made
    // again by the backend whenever that size changes (an HDR camera's picture, a post-processing step's input).
    std::optional<TargetSize> size;
    TextureFormat format = TextureFormat::Rgba8Unorm;
    // How a draw that samples it reads between its pixels.
    TextureFilter filter = TextureFilter::Linear;

    friend bool operator==(const RenderTargetDesc&, const RenderTargetDesc&) = default;
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

struct TextureTargetCreate final {
    TextureId id;
    RenderTargetDesc desc;
};

using TextureOp = std::variant<TextureCreate, TextureUpdate, TextureRelease, TextureTargetCreate>;

// render_world's Resource. The asks of a frame stay until render_world clears them after the frame was drawn (its
// Cleanup stage); a backend reads them in order before it draws. Ids are never given twice.
class RenderTextures final {
public:
    // A new texture with these pixels. A zero width or height, or pixels of another size, is a Programmer Error.
    TextureId Create(TextureDesc desc, std::vector<std::uint8_t> pixels);
    // Replaces a region of a texture. An id that is not live (never made, or let go), a region not inside the
    // texture or of zero size, or pixels of another size, is a Programmer Error.
    void Update(TextureId id, TextureRegion region, std::vector<std::uint8_t> pixels);
    // Lets the texture (or the target) go; draws and passes must not name it any more. An id that is not live is a
    // Programmer Error.
    void Release(TextureId id);
    // A new render target: passes name it by the id's value as their target, draws sample it by the id. A fixed size
    // of zero width or height is a Programmer Error. Its picture is undefined until a pass of the frame draws on it
    // (the first pass of a frame on a target with no clear starts from transparent black, as the main target's).
    TextureId CreateTarget(RenderTargetDesc desc);

    // The live texture's description, or nullptr (also for a target).
    [[nodiscard]] const TextureDesc* Find(TextureId id) const noexcept;
    // The live target's description, or nullptr (also for a texture made from pixels).
    [[nodiscard]] const RenderTargetDesc* FindTarget(TextureId id) const noexcept;
    // Textures and targets.
    [[nodiscard]] std::size_t LiveCount() const noexcept { return live_.size() + targets_.size(); }
    // This frame's asks, in the order they were made.
    [[nodiscard]] std::span<const TextureOp> Pending() const noexcept { return pending_; }
    // render_world, after each frame was drawn (Cleanup).
    void ClearPending() noexcept { pending_.clear(); }

private:
    std::uint32_t next_ = 1;
    std::vector<std::pair<std::uint32_t, TextureDesc>> live_;
    std::vector<std::pair<std::uint32_t, RenderTargetDesc>> targets_;
    std::vector<TextureOp> pending_;
};

inline void HashValue(Kernel::Hasher&, const RenderTextures&) {}

} // namespace GYO::Engine::Render
