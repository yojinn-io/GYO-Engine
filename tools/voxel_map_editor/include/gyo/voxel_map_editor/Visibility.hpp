#pragma once

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Frustum.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec3.hpp"

// Which preview chunks a frame draws (docs/tools/plans/v8/voxel-editor-cull.md):
// the chunks within a horizontal draw distance of the point being edited (the
// orbit target, so zooming out does not empty the view) that overlap the view
// frustum. Culling only chooses what one frame draws; what a chunk draws is
// still what saving writes (Design Invariant 7).
namespace Gyo::Tools::VoxelMapEditor {

inline constexpr float DefaultDrawDistanceCells = 256.0F;
inline constexpr float MinDrawDistanceCells = 64.0F;
inline constexpr float MaxDrawDistanceCells = 1024.0F;

struct ChunkView final {
    Engine::Math::Frustum frustum{};
    Engine::Math::Vec3 center{};  // world space; only x and z are used
    float drawDistance{};         // world units, measured in x and z
};

// clip is the matrix the chunks are drawn with (Engine::Render::CameraViewProjection).
[[nodiscard]] ChunkView MakeChunkView(const Engine::Math::Matrix4& clip, Engine::Math::Vec3 center,
                                      float drawDistance) noexcept;
// True when the box is within the draw distance (touching counts) and overlaps
// the frustum (Engine::Math::Intersects, conservative).
[[nodiscard]] bool Draws(const ChunkView& view, const Engine::Math::Aabb& box) noexcept;

} // namespace Gyo::Tools::VoxelMapEditor
