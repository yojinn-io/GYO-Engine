#pragma once

// world_render (docs/architecture/target/v2/render.zh-Hant.md, section 7): the source World end of render (the
// world_<x> naming of world_asset and world_time; Bevy's SpritePlugin and the camera's render settings). In the World
// it joins (a simulation or document World; default=no) it registers render's drawables, Sprite, Shape2d, Mesh3d,
// SkinPose and RenderLayers, the cameras' ClearColorConfig and the ClearColor Resource (opaque black unless one is already there),
// so the render World's camera_render and sprite_render can extract them (a type they read that the source did not
// register is the kernel's R-E4). It runs no System: the author writes the drawables, transform places them, camera
// computes the cameras.

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::WorldRender {

class WorldRenderModule final : public Orchestration::Module {
public:
    WorldRenderModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::WorldRender
