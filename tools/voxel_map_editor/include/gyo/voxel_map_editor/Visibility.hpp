#pragma once

#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Frustum.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

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

// ve-feel-1 (docs/tools/plans/v8/voxel-editor-feel-1.md), F1: zoom and how far is drawn.

// Until region buffers (2) exist, a frame draws at most this many chunks (ve-cull's CB2 budget).
inline constexpr std::size_t MaxDrawnChunks = 2000;
// Zooming out draws this many times the camera's distance at least.
inline constexpr float DrawDistancePerCameraDistance = 1.5F;

// The filled chunks' bounds in relative cells (low inclusive, high exclusive); nullopt for a grid without cells.
[[nodiscard]] std::optional<std::pair<Vec3i, Vec3i>> ChunkBounds(const GridDocument& document);
// The camera distance that frames the whole map (the 0 key): 0.9 x the bounds' diagonal, at least 4 cells;
// a grid without cells frames a 16 x 4 x 16 box. World units.
[[nodiscard]] float FrameDistance(const std::optional<std::pair<Vec3i, Vec3i>>& bounds, float cellSize) noexcept;

struct ZoomRange final {
    float minimum{}; // 2 cells: details
    float maximum{}; // the whole map: FrameDistance, never below minimum
};
[[nodiscard]] ZoomRange ZoomLimits(const std::optional<std::pair<Vec3i, Vec3i>>& bounds, float cellSize) noexcept;
[[nodiscard]] float ClampZoom(float distance, const ZoomRange& range) noexcept;
// The distance a frame draws to: the panel's, or DrawDistancePerCameraDistance x the camera's distance when that
// is farther (world units).
[[nodiscard]] float DrawDistanceFor(float panelDistance, float cameraDistance) noexcept;

struct ChosenChunks final {
    std::vector<std::size_t> indices; // into the boxes, ascending
    bool capped{};                    // more were in view than maxChunks; the farthest are left out
};
// The boxes Draws keeps; when more than maxChunks, the maxChunks nearest the center in x and z (ties: the lower
// index).
[[nodiscard]] ChosenChunks ChooseChunks(const ChunkView& view, std::span<const Engine::Math::Aabb> boxes,
                                        std::size_t maxChunks);

} // namespace Gyo::Tools::VoxelMapEditor
