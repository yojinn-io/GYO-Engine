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

// How a texture made from pixels stores and reads its texels (Bevy's TextureFormat, of which these are the ones in use;
// the compressed ones come with F1). Its pixels are 4 bytes R, G, B, A: a float format is a render target's alone
// (RenderTargetFormat::Rgba16Float; a picture of float pixels, a sky's, comes with C3).
enum class TextureFormat : std::uint8_t {
    // Sampled as stored: the 2D pass's pictures (it blends bytes, RP-4), data pictures (the normal map _N, _ORM).
    Rgba8Unorm = 1,
    // The same bytes, R, G and B sRGB-encoded: sampling decodes them to linear (the colour pictures _D, _E that 3D
    // lights in linear space; alpha as stored). Bevy's default for colour pictures (ImageLoaderSettings::is_srgb,
    // bevy_image/src/image_loader.rs:148,176).
    Rgba8UnormSrgb,
};

// How a render target stores what passes draw on it. Its own type: a target of pixels' formats, a texture of a
// target's, cannot be written down.
enum class RenderTargetFormat : std::uint8_t {
    // R, G, B, A, 8 bits each: what colour passes draw, sampled as stored (its bytes are blended as they are). There is
    // no sRGB target (render-pipeline declaration 16): the linear path encodes in its tonemapping pass's shader and
    // writes bytes on an 8-bit target, where the 2D passes and the UI blend bytes (RP-4).
    Rgba8Unorm = 1,
    // Depth, 0 near to 1 far (render-pipeline declaration section 14): drawn on by depth-only passes and 3D passes
    // (Pass3D::depth), sampled (as a texture whose first channel is the depth) between its pixels by the nearest one
    // whatever its filter (a driver need not filter depth; Bevy binds its depth textures with non-filtering samplers).
    // The backend picks the stored format, at least 16 bits (Bevy's is Depth32Float, CORE_3D_DEPTH_FORMAT,
    // bevy_core_pipeline/src/core_3d/mod.rs:5; SDL_GPU guarantees only D16 for both drawing and sampling,
    // SDL_gpu.h:703,747).
    Depth,
    // R, G, B, A, a 16-bit float each: linear light, values past 1 kept (a Hdr camera's picture, Tonemapping.hpp;
    // Bevy's Rgba16Float main texture, bevy_render/src/camera.rs:799-801). A colour is bytes as stored (sRGB-encoded,
    // Color8): a clear colour on it is decoded to the light it stands for (as Bevy converts a clear colour to its main
    // texture's space, bevy_render/src/view/mod.rs:1442-1450); the unlit 3D pass writes the light its bytes stand for;
    // the 2D pass writes its bytes / 255 as they are (2D blends bytes, RP-4: the 2D pipelines draw on no such target).
    // Sampled as stored.
    Rgba16Float,
};

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
    RenderTargetFormat format = RenderTargetFormat::Rgba8Unorm;
    // How a draw that samples a colour target reads between its pixels (a Depth target: always the nearest).
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

// A target's size from this frame on (after its TextureTargetCreate): the backend makes its texture at this size, or
// makes it again, with no picture (render-pipeline declaration 14.11).
struct TextureTargetSize final {
    TextureId id;
    TargetSize size;
};

using TextureOp = std::variant<TextureCreate, TextureUpdate, TextureRelease, TextureTargetCreate, TextureTargetSize>;

struct RenderCommands;

// The frame's receipt: a buffer the backend writes each frame and the data side reads and then clears, recording the
// render targets actually drawn on and the sizes actually applied this frame; one entry per target. render_world's
// Resource: the backend writes it in the Submit stage (the one thing it writes, SDL-5), render_world's Cleanup of the
// same frame reads it (RenderTextures::EndFrame) and then clears it (render-pipeline declaration 14.13). A frame the backend
// did not get done (no backend, a failed upload or submission) leaves no target drawn; a size it did not apply
// (refused) is not in it, and is asked for again the next frame.
class RenderFrameReceipt final {
public:
    // A pass of this frame drew on the target (as colour or as depth) and the frame was handed to the GPU.
    void RecordDrawn(TextureId target);
    // The target's texture is now this size (made at it); a later size of the same frame replaces it.
    void RecordApplied(TextureId target, TargetSize size);
    [[nodiscard]] std::span<const TextureId> DrawnTargets() const noexcept { return drawn_; }
    [[nodiscard]] std::span<const TextureTargetSize> AppliedSizes() const noexcept { return applied_; }
    [[nodiscard]] bool Empty() const noexcept { return drawn_.empty() && applied_.empty(); }
    void Clear() noexcept;

private:
    std::vector<TextureId> drawn_;
    std::vector<TextureTargetSize> applied_;
};

inline void HashValue(Kernel::Hasher&, const RenderFrameReceipt&) {}

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
    // Programmer Error. A target that follows it keeps its last size (it has no size to follow).
    void Release(TextureId id);
    // A new render target: passes name it as their target, draws sample it by the id. Its size is decided here, on
    // the data side (render-pipeline declaration 14.11): fixed, the main target's (SetMainSize) or another target's,
    // each change asked of the backend as a TextureTargetSize. Its texture is kept from frame to frame (a pass with no
    // clear draws over it, as Bevy's LoadOp::Load); it has no picture when it is made and whenever its size changes,
    // until the backend draws on it (RenderFrameReceipt, EndFrame): the first pass that draws on it then starts from
    // transparent black (a Depth target: cleared to 1). A pass samples it only after a pass of the same frame drew on
    // it, or the frame retains it after a frame drew on it at its size (WasDrawn; RenderCommands.hpp, RP-3). A fixed
    // size of zero width or height, or following an id that is not a live target, is a Programmer Error.
    TextureId CreateTarget(RenderTargetDesc desc);
    // The main target's size this frame (the window's, as the source World's cameras computed with it; camera_render
    // sets it in the extract; a render World without cameras sets it itself): targets that follow it take it.
    // nullopt: not known (they keep the size they have).
    void SetMainSize(std::optional<TargetSize> size);
    // After a frame was drawn (render_world's Cleanup; the one way a frame ends): what the frame's passes drew on
    // (WasDrawn: the program's account, RP-3 and retaining go by it), what the backend got done (the receipt:
    // HasPicture, the sizes applied; render_world clears it after), then this frame's asks are gone and the next
    // frame's begin: a
    // target whose size was not applied is asked for again, whole (its making, then its size), each frame until it
    // is (render-pipeline declaration 14.13, 14.14).
    void EndFrame(const RenderCommands& commands, const RenderFrameReceipt& receipt);

    // The live target's size as decided here, or nullopt (not known yet: it follows a size not known; or not a live
    // target).
    [[nodiscard]] std::optional<TargetSize> SizeOf(TextureId id) const noexcept;
    // The main target's size as last set.
    [[nodiscard]] std::optional<TargetSize> MainSize() const noexcept { return main_; }
    // Whether a frame's passes drew on the live target at its size since it was made or its size last changed: what
    // the program did, whatever the GPU made of it. A frame may retain it (RP-3).
    [[nodiscard]] bool WasDrawn(TextureId id) const noexcept;
    // Whether the live target has a picture: the backend drew on it at its size, and said so in a receipt, since it
    // was made or its size last changed. A camera that does not update draws a target without one again.
    [[nodiscard]] bool HasPicture(TextureId id) const noexcept;
    // The same for a pass target: the main target (nullopt) keeps no picture from frame to frame.
    [[nodiscard]] bool HasPicture(const std::optional<TextureId>& target) const noexcept { return target && HasPicture(*target); }

    // The live texture's description, or nullptr (also for a target).
    [[nodiscard]] const TextureDesc* Find(TextureId id) const noexcept;
    // The live target's description, or nullptr (also for a texture made from pixels).
    [[nodiscard]] const RenderTargetDesc* FindTarget(TextureId id) const noexcept;
    // Textures and targets.
    [[nodiscard]] std::size_t LiveCount() const noexcept { return live_.size(); }
    // This frame's asks, in the order they were made.
    [[nodiscard]] std::span<const TextureOp> Pending() const noexcept { return pending_; }
private:
    // EndFrame's steps, in this order: the receipt before the asks are cleared (a size applied is not asked for again).
    void Apply(const RenderFrameReceipt& receipt);
    void ClearPending();

    // Each target's size as decided here, the size the backend last applied (none since a new size was asked: the
    // backend lets the old texture go first, as Bevy's prepare_assets drops the old asset), whether this frame's asks
    // hold its size, whether its last ask went unanswered (asked for again whole), whether a frame drew on it, and its
    // picture; in id order.
    struct TargetState {
        std::uint32_t id = 0;
        std::optional<TargetSize> size;
        std::optional<TargetSize> applied;
        bool asked = false;
        bool unanswered = false;
        bool drawn = false;
        bool picture = false;
    };
    // Gives every target the size its description says now (in id order: a followed target before its follower); a
    // change loses the picture. A size the backend has not applied is asked for, once a frame.
    void ResolveSizes();

    std::uint32_t next_ = 1;
    // One table for both: one id space, one counter (RD-3).
    std::vector<std::pair<std::uint32_t, std::variant<TextureDesc, RenderTargetDesc>>> live_;
    std::vector<TargetState> targets_;
    std::optional<TargetSize> main_;
    std::vector<TextureOp> pending_;
};

inline void HashValue(Kernel::Hasher&, const RenderTextures&) {}

} // namespace GYO::Engine::Render
