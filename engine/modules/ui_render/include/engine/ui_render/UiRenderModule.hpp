#pragma once

// ui_render (docs/architecture/target/v2/ui.zh-Hant.md, section 4.7; Bevy's bevy_ui_render): the UI of the render World
// (with extractFrom; render_world and camera_render before it).
//
// - ExtractUi, in render_world's extract schedule: rebuilds ExtractedUiRoots from the source World's UiRoot and
//   UiRootState (roots with a camera, no failure and a draw list), uploads what changed in the source's UiText atlas
//   since the last frame (its pages as RenderTextures, Alpha8 coverage as white with that alpha, nearest filtering),
//   and makes the textures of the pictures through RenderImages (a picture not loaded yet: that image is skipped).
//   The render World only reads the source (UI-7, DI-KS 13).
// - QueueUiPasses, in UiRenderOptions::schedule / stage (Main / PostUpdate, before camera_render's
//   ComposeCameraPasses): for each extracted camera, the roots drawn over it by (order, entity) as one Pass2D on the
//   camera's viewport (view: its own pixels), at the end of the camera's slot of CameraPasses, so the UI lies over
//   what the camera's 2D and 3D passes drew (Bevy's TransparentUi phase after the main pass).
//
// Colours: ui's linear colours are encoded to sRGB bytes (the 2D pass blends stored bytes, render.zh-Hant.md section 2).

#include <cstdint>
#include <string>
#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/ui/UiTypes.hpp"

namespace GYO::Engine::UiRender {

// One root as this frame's extract saw it.
struct ExtractedUiRoot final {
    std::uint64_t entity = 0;
    // The source camera entity it is drawn over.
    std::uint64_t camera = 0;
    std::int32_t order = 0;
    Ui::UiDrawList drawList;
    // The texture of each image command's picture, in the order of the draw list's image commands; TextureId{} for a
    // picture not loaded yet (that image is skipped).
    std::vector<Render::TextureId> images;
};

// ui_render's Resource: this frame's roots.
struct ExtractedUiRoots final {
    std::vector<ExtractedUiRoot> roots;
};

// ui_render's Resource: the textures of the atlas pages and the atlas revision last uploaded.
struct UiAtlasTextures final {
    std::uint64_t revision = 0;
    std::vector<Render::TextureId> pages;
    std::vector<Render::TextureDesc> sizes;
};

inline void HashValue(Kernel::Hasher&, const ExtractedUiRoots&) {}
inline void HashValue(Kernel::Hasher&, const UiAtlasTextures&) {}

// Where QueueUiPasses runs (WorldDescription::options; none given: the default skeleton).
struct UiRenderOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::postUpdate);
};

class UiRenderModule final : public Orchestration::Module {
public:
    UiRenderModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::UiRender
