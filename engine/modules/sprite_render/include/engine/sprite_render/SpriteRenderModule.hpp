#pragma once

// sprite_render (docs/architecture/target/v2/render.zh-Hant.md, section 7; Bevy's bevy_sprite_render): the 2D items of
// the render World (with extractFrom; render_world and camera_render before it).
//
// - ExtractSprites2d, in render_world's extract schedule: rebuilds render's Extracted2dItems from the source World's
//   Sprites and Shape2ds that have a GlobalTransform (RenderLayers: layer 0 when none), and makes the textures of
//   their images through RenderImages and RenderTextures (an image asset not loaded yet: the item is skipped this
//   frame). Then the textures of held images no item showed are let go.
// - Queue2dPasses, in SpriteRenderOptions::schedule / stage (Main / Update): for each extracted 2D camera, in order,
//   appends Queue2d's pass to RenderCommands (before render_world's Submit stage).
//
// The source World needs transform and world_render (else R-E4); an image asset is read through the source's
// AssetView (world_asset), so a handle loaded in the source this frame is drawn this frame.

#include <string>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::SpriteRender {

// Where Queue2dPasses runs (WorldDescription::options; none given: the default skeleton).
struct SpriteRenderOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::update);
};

class SpriteRenderModule final : public Orchestration::Module {
public:
    SpriteRenderModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::SpriteRender
