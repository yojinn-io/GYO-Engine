#include "gyo/voxel_map_editor/Visibility.hpp"

#include "engine/math/scalar/Scalar.hpp"

#include <algorithm>

namespace Gyo::Tools::VoxelMapEditor {

namespace Math = Engine::Math;

ChunkView MakeChunkView(const Math::Matrix4& clip, const Math::Vec3 center, const float drawDistance) noexcept {
    return {Math::MakeFrustum(clip), center, drawDistance};
}

namespace {

// Squared distance in x and z from the center to the box's nearest point.
float FlatDistance2(const Math::Vec3 center, const Math::Aabb& box) noexcept {
    const float dx = Math::Max(Math::Max(box.minimum.x - center.x, center.x - box.maximum.x), 0.0F);
    const float dz = Math::Max(Math::Max(box.minimum.z - center.z, center.z - box.maximum.z), 0.0F);
    return dx * dx + dz * dz;
}

} // namespace

bool Draws(const ChunkView& view, const Math::Aabb& box) noexcept {
    if (FlatDistance2(view.center, box) > view.drawDistance * view.drawDistance) return false;
    return Math::Intersects(view.frustum, box);
}

std::optional<std::pair<Vec3i, Vec3i>> ChunkBounds(const GridDocument& document) {
    std::optional<std::pair<Vec3i, Vec3i>> bounds;
    for (const auto& [at, chunk] : document.chunks) {
        Vec3i low{};
        Vec3i high{};
        for (int axis = 0; axis < 3; ++axis) {
            low[axis] = at[axis] * ChunkSize;
            high[axis] = low[axis] + ChunkSize;
        }
        if (!bounds) {
            bounds = {low, high};
            continue;
        }
        for (int axis = 0; axis < 3; ++axis) {
            bounds->first[axis] = Math::Min(bounds->first[axis], low[axis]);
            bounds->second[axis] = Math::Max(bounds->second[axis], high[axis]);
        }
    }
    return bounds;
}

float FrameDistance(const std::optional<std::pair<Vec3i, Vec3i>>& bounds, const float cellSize) noexcept {
    Math::Vec3 extent{16.0F, 4.0F, 16.0F};
    if (bounds)
        extent = {static_cast<float>(bounds->second[0] - bounds->first[0]),
                  static_cast<float>(bounds->second[1] - bounds->first[1]),
                  static_cast<float>(bounds->second[2] - bounds->first[2])};
    return Math::Max(Math::Length(extent) * cellSize * 0.9F, 4.0F * cellSize);
}

ZoomRange ZoomLimits(const std::optional<std::pair<Vec3i, Vec3i>>& bounds, const float cellSize) noexcept {
    const float minimum = 2.0F * cellSize;
    return {minimum, Math::Max(FrameDistance(bounds, cellSize), minimum)};
}

float ClampZoom(const float distance, const ZoomRange& range) noexcept {
    return Math::Clamp(distance, range.minimum, range.maximum);
}

float DrawDistanceFor(const float panelDistance, const float cameraDistance) noexcept {
    return Math::Max(panelDistance, cameraDistance * DrawDistancePerCameraDistance);
}

ChosenChunks ChooseChunks(const ChunkView& view, const std::span<const Math::Aabb> boxes, const std::size_t maxChunks) {
    ChosenChunks chosen;
    for (std::size_t index = 0; index < boxes.size(); ++index)
        if (Draws(view, boxes[index])) chosen.indices.push_back(index);
    if (chosen.indices.size() <= maxChunks) return chosen;
    chosen.capped = true;
    // The nearest first, ties by index, so the choice is the same on every platform.
    std::vector<std::pair<float, std::size_t>> keyed;
    keyed.reserve(chosen.indices.size());
    for (const std::size_t index : chosen.indices) keyed.emplace_back(FlatDistance2(view.center, boxes[index]), index);
    std::nth_element(keyed.begin(), keyed.begin() + static_cast<std::ptrdiff_t>(maxChunks), keyed.end());
    chosen.indices.clear();
    for (std::size_t i = 0; i < maxChunks; ++i) chosen.indices.push_back(keyed[i].second);
    std::sort(chosen.indices.begin(), chosen.indices.end());
    return chosen;
}

} // namespace Gyo::Tools::VoxelMapEditor
