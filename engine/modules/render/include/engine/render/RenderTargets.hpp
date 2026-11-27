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

} // namespace GYO::Engine::Render
