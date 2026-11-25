#pragma once

// mesh_render (docs/architecture/target/v2/render.zh-Hant.md, section 8; batch r2b-3d): the 3D items of the render
// World (with extractFrom; render_world and camera_render before it).
//
// - ExtractMeshes3d, in render_world's extract schedule: rebuilds render's Extracted3dItems from the source World's
//   Mesh3ds that have a GlobalTransform and whose model is loaded (read through the source's AssetView): a mesh
//   without a skin becomes its retained mesh (RenderModelMeshes, made on RenderMeshes the first time) placed at its
//   node (the entity's SkinPose, else the rest pose); a skinned mesh is skinned on the CPU with the entity's SkinPose
//   (else the rest pose) into this frame's vertices. Images through render_world's RenderImages, as sprite_render. An
//   entity with an Aabb or a VisibilityRange is marked visibilityChecked: Queue3d draws it for a camera only when the
//   camera's VisibleEntities (the visibility Module's) lists it.
// - Queue3dPasses, in MeshRenderOptions::schedule / stage (Main / Update): puts each extracted 3D camera's Queue3d
//   pass in its slot of CameraPasses; camera_render composes them.
//
// The source World needs transform, camera, world_render and world_asset (else R-E4).

#include <string>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::MeshRender {

// Where Queue3dPasses runs (WorldDescription::options; none given: the default skeleton).
struct MeshRenderOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::update);
};

class MeshRenderModule final : public Orchestration::Module {
public:
    MeshRenderModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::MeshRender
