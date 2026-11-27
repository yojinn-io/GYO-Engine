#pragma once

// Render targets a source World's cameras draw into (docs/architecture/target/v2/render-pipeline-declaration.zh-Hant.md,
// section 13; render.zh-Hant.md section 3). Bevy's camera names an Image of the main world as its RenderTarget::Image
// (bevy_camera/src/camera.rs:896) and camera_system reads the size from Assets<Image> (bevy_render/src/camera.rs:292);
// the render world makes the GPU texture keyed by the image's id. Here the same split, in the two Worlds:
//
// - RenderTargets (the source World's Resource, registered by world_render): the data side hands out the id an author
//   puts in a camera's CameraTarget, from 1, never twice (as RenderTextures' ids, RD-3), and keeps each live
//   target's description. world_render copies each target's size into camera's CameraTargetSizes (CM-5).
// - TargetTextures (the render World's Resource, registered by camera_render): the render target of RenderTextures
//   made for each source target, kept in step by SyncTargetTextures in the extract; a camera's passes go to it.
//
// A render World that draws without a source (a single World writing RenderCommands itself) makes its targets with
// RenderTextures::CreateTarget and needs neither.

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Engine::Render {

class RenderTargets final {
public:
    // A new target with this description; its id is what a camera's CameraTarget names. A fixed size of zero width or
    // height is a Programmer Error.
    Camera::CameraTarget Create(RenderTargetDesc desc);
    // Lets the target go; cameras must not name it any more. A target that is not live is a Programmer Error.
    void Release(Camera::CameraTarget target);

    // The live target's description, or nullptr (also for 0, the main target).
    [[nodiscard]] const RenderTargetDesc* Find(Camera::CameraTarget target) const noexcept;
    // The live targets, by id.
    [[nodiscard]] std::span<const std::pair<std::uint32_t, RenderTargetDesc>> Live() const noexcept { return live_; }

private:
    std::uint32_t next_ = 1;
    std::vector<std::pair<std::uint32_t, RenderTargetDesc>> live_;
};

// The author's data for the render World to draw, not decided state (as the drawables, Drawables.hpp).
inline void HashValue(Kernel::Hasher&, const RenderTargets&) {}

// The size camera's CameraTargetSizes should hold for a target: a fixed size with scale factor 1 (Bevy's
// ImageRenderTarget::scale_factor default), or the main target's size and scale factor; nullopt when the target
// follows the main target and main is null (the window's size is not known yet).
[[nodiscard]] std::optional<Camera::TargetSize> TargetSizeOf(const RenderTargetDesc& desc, const Camera::TargetSize* main);

// The render World's targets made for the source's.
struct TargetTextures final {
    struct Entry {
        // The source's id (CameraTarget::id).
        std::uint32_t source = 0;
        RenderTargetDesc desc;
        TextureId texture{};
    };
    // By source id.
    std::vector<Entry> entries;

    // The texture made for the source's target, or nullptr.
    [[nodiscard]] const TextureId* Find(std::uint32_t source) const noexcept;
};

inline void HashValue(Kernel::Hasher&, const TargetTextures&) {}

// Brings mapped in step with the source's targets (null: the source has none): a target seen for the first time, or
// whose description changed (a replaced source World gives its ids again from 1), gets a new render target of
// textures (the old one let go); a mapped target the source no longer has is let go.
void SyncTargetTextures(const RenderTargets* source, RenderTextures& textures, TargetTextures& mapped);

// The pass target a camera of this CameraTarget draws on: 0 for 0; the mapped texture's id for a mapped target;
// nullopt for an id the source has no target for (such a camera is not extracted: it has nothing to draw on).
[[nodiscard]] std::optional<std::uint32_t> PassTargetOf(std::uint32_t cameraTarget, const TargetTextures& mapped);

} // namespace GYO::Engine::Render
