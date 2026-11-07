#include "gyo/voxel_map_editor/Visibility.hpp"

#include "engine/math/scalar/Scalar.hpp"

namespace Gyo::Tools::VoxelMapEditor {

namespace Math = Engine::Math;

ChunkView MakeChunkView(const Math::Matrix4& clip, const Math::Vec3 center, const float drawDistance) noexcept {
    return {Math::MakeFrustum(clip), center, drawDistance};
}

bool Draws(const ChunkView& view, const Math::Aabb& box) noexcept {
    // The box's nearest point to the center in x and z.
    const float dx = Math::Max(Math::Max(box.minimum.x - view.center.x, view.center.x - box.maximum.x), 0.0F);
    const float dz = Math::Max(Math::Max(box.minimum.z - view.center.z, view.center.z - box.maximum.z), 0.0F);
    if (dx * dx + dz * dz > view.drawDistance * view.drawDistance) return false;
    return Math::Intersects(view.frustum, box);
}

} // namespace Gyo::Tools::VoxelMapEditor
