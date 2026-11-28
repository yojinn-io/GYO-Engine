#pragma once

// Render targets a source World's cameras draw into (docs/architecture/target/v2/render-pipeline-declaration.zh-Hant.md,
// section 13; render.zh-Hant.md section 3). Bevy's camera names an Image of the main world as its RenderTarget::Image
// (bevy_camera/src/camera.rs:896) and camera_system reads the size from Assets<Image> (bevy_render/src/camera.rs:292);
// the render world makes the GPU texture keyed by the image's id. Here the same split, in the two Worlds:
//
// - RenderTargets (the source World's Resource, registered by world_render): the one owner of every camera target id
//   but the main one (Camera::kMainTarget). It hands out the id an author puts in a camera's CameraTarget, from 1,
//   never twice (as RenderTextures' ids, RD-3), and keeps each live target's description. world_render copies each
//   target's size into camera's CameraTargetSizes (CM-5).
// - TargetTextures (the render World's Resource, registered by camera_render): the render target of RenderTextures
//   made for each source target, kept in step by SyncTargetTextures in the extract; a camera's passes go to it.
//
// - HdrTargets (the render World's Resource, registered by camera_render): the targets the render World makes for its
//   Hdr cameras itself (render-pipeline declaration section 16), kept in step by SyncHdrTargets in the extract.
//
// A render World that draws without a source (a single World writing RenderCommands itself) makes its targets with
// RenderTextures::CreateTarget and needs neither.

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Engine::Render {

// A source target: its size fixed, or another camera target's (the main target, or a source target made before it),
// and how a draw that samples it reads. Rgba8Unorm, as every render target for now (TextureFormat).
struct SourceTargetDesc final {
    std::variant<TargetSize, Camera::CameraTarget> size = Camera::kMainTarget;
    TextureFilter filter = TextureFilter::Linear;

    friend bool operator==(const SourceTargetDesc&, const SourceTargetDesc&) = default;
};

class RenderTargets final {
public:
    RenderTargets();

    // A new target with this description; its id is what a camera's CameraTarget names. A fixed size of zero width or
    // height, or following an id that is neither the main target nor a live target, is a Programmer Error.
    Camera::CameraTarget Create(SourceTargetDesc desc);
    // Lets the target go; cameras must not name it any more. A target that is not live is a Programmer Error.
    void Release(Camera::CameraTarget target);

    // The live target's description, or nullptr (also for the main target).
    [[nodiscard]] const SourceTargetDesc* Find(Camera::CameraTarget target) const noexcept;
    // The live targets, by id.
    [[nodiscard]] std::span<const std::pair<std::uint32_t, SourceTargetDesc>> Live() const noexcept { return live_; }
    // Which table this is: a new RenderTargets (a new source World) gets a new one; a copy (carried over, O-7) keeps it.
    // The render World drops what it made for another table, so a replaced World's ids, given again from 1, never
    // name the old targets.
    [[nodiscard]] std::uint64_t Table() const noexcept { return table_; }

private:
    std::uint64_t table_;
    std::uint32_t next_ = 1;
    std::vector<std::pair<std::uint32_t, SourceTargetDesc>> live_;
};

// The author's data for the render World to draw, not decided state (as the drawables, Drawables.hpp): the camera's
// CameraTarget is hashed as the camera's setting, the table that gives it is not.
inline void HashValue(Kernel::Hasher&, const RenderTargets&) {}

// The size camera's CameraTargetSizes should hold for a target: a fixed size with scale factor 1 (Bevy's
// ImageRenderTarget::scale_factor default), or the followed target's size and scale factor in sizes; nullopt when
// that one has no size (the window's is not known yet).
[[nodiscard]] std::optional<Camera::TargetSize> TargetSizeOf(const SourceTargetDesc& desc, const Camera::CameraTargetSizes& sizes);

// The render World's targets made for the source's.
struct TargetTextures final {
    struct Entry {
        // The source's id (CameraTarget::id) and the render target made for it.
        std::uint32_t source = 0;
        TextureId texture{};
    };
    // The source table they were made for (RenderTargets::Table); 0 before the first.
    std::uint64_t table = 0;
    // By source id.
    std::vector<Entry> entries;

    // The texture made for the source's target, or nullptr (also for the main target).
    [[nodiscard]] const TextureId* Find(Camera::CameraTarget source) const noexcept;
};

inline void HashValue(Kernel::Hasher&, const TargetTextures&) {}

// Brings mapped in step with the source's targets (null: the source has none). Another table than the last (a replaced
// source World) lets go of every target made for the old one: its ids, given again from 1, are other targets. A target
// seen for the first time gets a new render target of textures (one that follows a source target follows the render
// target made for it); a mapped target the source no longer has is let go, and so is one following a target the source
// let go.
void SyncTargetTextures(const RenderTargets* source, RenderTextures& textures, TargetTextures& mapped);

// A 3D camera with Hdr drawing this frame, and the pass target its own target maps to (nullopt: the main target).
struct HdrCamera final {
    std::uint64_t camera = 0;
    std::optional<TextureId> output;
};

// The render World's targets for its Hdr cameras (render-pipeline declaration 16, revision 16.8), all following their
// camera target's size:
// - a picture per Hdr camera (Rgba16Float, read at its own texels: the nearest): the camera's phases up to PostProcess
//   draw on it alone, so its tonemapping puts on the target what this camera drew and nothing of the cameras before it
//   (they keep what they drew under it, as without Hdr). Bevy shares one main texture per target among its cameras
//   (bevy_render/src/view/mod.rs:1385), whose UI is drawn into it; here the UI draws bytes on the target (RP-4).
// - a depth per camera target with a Hdr camera on it (Depth), which every 3D camera on that target tests and writes
//   (Pass3D::depth), Hdr or not: one depth per target, as Bevy's ViewDepthTexture keyed by (target, msaa)
//   (bevy_core_pipeline/src/core_3d/mod.rs:699-700), so a viewmodel camera that keeps the depth (Camera3dDepth{false})
//   keeps the world camera's.
struct HdrTargets final {
    struct Picture {
        std::uint64_t camera = 0;
        std::optional<TextureId> output;
        TextureId picture{};
    };
    struct Depth {
        std::optional<TextureId> output;
        TextureId depth{};
    };
    std::vector<Picture> pictures;
    std::vector<Depth> depths;

    [[nodiscard]] std::optional<TextureId> PictureOf(std::uint64_t camera) const noexcept;
    [[nodiscard]] std::optional<TextureId> DepthOf(const std::optional<TextureId>& output) const noexcept;
};

inline void HashValue(Kernel::Hasher&, const HdrTargets&) {}

// Keeps a picture for each of cameras (made the first time, kept while the camera draws on the same target) and a depth
// for each camera target they draw on, and lets go of the others: a camera that does not draw this frame, a target no
// Hdr camera draws on, has none.
void SyncHdrTargets(std::span<const HdrCamera> cameras, RenderTextures& textures, HdrTargets& targets);

} // namespace GYO::Engine::Render
