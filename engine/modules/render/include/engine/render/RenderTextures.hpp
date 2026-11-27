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

// How a texture's texels are stored and read (Bevy's TextureFormat, of which these are the ones in use; a format comes
// with the batch that needs it: Rgba16Float with HDR (A4), the compressed ones with F1).
enum class TextureFormat : std::uint8_t {
    // R, G, B, A, 8 bits each, sampled as stored: the 2D pass's pictures (it blends bytes, RP-4), data pictures (the
    // normal map _N, _ORM), every render target (its bytes are blended as they are).
    Rgba8Unorm = 1,
    // The same bytes, R, G and B sRGB-encoded: sampling decodes them to linear (the colour pictures _D, _E that 3D
    // lights in linear space; alpha as stored). Bevy's default for colour pictures (ImageLoaderSettings::is_srgb,
    // bevy_image/src/image_loader.rs:148,176). Not a render target's format yet: what a clear colour and blending mean
    // on one comes with the linear path (A4).
    Rgba8UnormSrgb,
    // Depth, 0 near to 1 far (render-pipeline declaration section 14): a render target's only, drawn on by depth-only
    // passes and 3D passes (Pass3D::depth) and sampled (as a texture whose first channel is the depth) with the Nearest
    // filter. The backend picks the stored format, at least 16 bits (Bevy's is Depth32Float, CORE_3D_DEPTH_FORMAT,
    // bevy_core_pipeline/src/core_3d/mod.rs:5; SDL_GPU guarantees only D16 for both drawing and sampling,
    // SDL_gpu.h:703,747).
    Depth,
};

struct TextureDesc final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    TextureFilter filter = TextureFilter::Linear;
    // Rgba8Unorm by default: the bytes are sampled as stored, as every texture was before formats (the 2D pass and the
    // unlit 3D pass blend bytes). A picture one draw needs decoded and another as stored is two textures: SDL_GPU 3.4
    // makes each texture with one format and has no view of another format (render-pipeline declaration 13.1). The
    // pixels are 4 bytes R, G, B, A: Rgba8Unorm or Rgba8UnormSrgb (Depth is a render target's only).
    TextureFormat format = TextureFormat::Rgba8Unorm;

    friend bool operator==(const TextureDesc&, const TextureDesc&) = default;
};

// A target's size in pixels.
struct TargetSize final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    friend bool operator==(const TargetSize&, const TargetSize&) = default;
};

// A target whose size is another target's: the main target (nullopt: the window, or the picture a backend captures) or
// a render target of the same RenderTextures. The backend makes it again whenever that size changes (an HDR camera's
// picture follows the camera's target, a post-processing step's input follows its output; Bevy's ViewTarget textures
// take the camera's physical target size, bevy_render/src/view/mod.rs:1416).
struct FollowTarget final {
    std::optional<TextureId> target;

    friend bool operator==(const FollowTarget&, const FollowTarget&) = default;
};

// A render target: a texture passes draw into and later draws sample.
struct RenderTargetDesc final {
    // Its size: fixed, or another target's.
    std::variant<TargetSize, FollowTarget> size = FollowTarget{};
    // Rgba8Unorm, or Depth (TextureFormat).
    TextureFormat format = TextureFormat::Rgba8Unorm;
    // How a draw that samples it reads between its pixels; Nearest for a Depth target (a driver need not filter
    // depth; Bevy binds its depth textures with non-filtering samplers).
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
    // A new texture with these pixels. A zero width or height, pixels of another size, or the format Depth, is a
    // Programmer Error.
    TextureId Create(TextureDesc desc, std::vector<std::uint8_t> pixels);
    // Replaces a region of a texture. An id that is not live (never made, or let go), a region not inside the
    // texture or of zero size, or pixels of another size, is a Programmer Error.
    void Update(TextureId id, TextureRegion region, std::vector<std::uint8_t> pixels);
    // Lets the texture (or the target) go; draws and passes must not name it any more. An id that is not live is a
    // Programmer Error. A target that follows it has no picture from then on.
    void Release(TextureId id);
    // A new render target: passes name it as their target, draws sample it by the id. Its picture is kept from frame
    // to frame (a pass with no clear draws over it, as Bevy's LoadOp::Load); it has none when it is made and when the
    // backend makes it again at a new size: the first pass that draws on it then starts from transparent black (a
    // Depth target: cleared to 1), and sampling it before that is a Programmer Error (RenderCommands.hpp, RP-3). A
    // fixed size of zero width or height, a format other than Rgba8Unorm or Depth, a Depth target not sampled Nearest,
    // or following an id that is not a live target, is a Programmer Error.
    TextureId CreateTarget(RenderTargetDesc desc);

    // The live texture's description, or nullptr (also for a target).
    [[nodiscard]] const TextureDesc* Find(TextureId id) const noexcept;
    // The live target's description, or nullptr (also for a texture made from pixels).
    [[nodiscard]] const RenderTargetDesc* FindTarget(TextureId id) const noexcept;
    // Textures and targets.
    [[nodiscard]] std::size_t LiveCount() const noexcept { return live_.size(); }
    // This frame's asks, in the order they were made.
    [[nodiscard]] std::span<const TextureOp> Pending() const noexcept { return pending_; }
    // render_world, after each frame was drawn (Cleanup).
    void ClearPending() noexcept { pending_.clear(); }

private:
    std::uint32_t next_ = 1;
    // One table for both: one id space, one counter (RD-3).
    std::vector<std::pair<std::uint32_t, std::variant<TextureDesc, RenderTargetDesc>>> live_;
    std::vector<TextureOp> pending_;
};

inline void HashValue(Kernel::Hasher&, const RenderTextures&) {}

} // namespace GYO::Engine::Render
