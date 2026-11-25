#include "engine/visibility/Visibility.hpp"

#include <algorithm>
#include <array>

#include "engine/math/geometry/Frustum.hpp"

namespace GYO::Engine::Visibility {

Math::Aabb WorldBox(const Math::Aabb& local, const Math::Matrix4& world) noexcept {
    const Math::Vec3& a = local.minimum;
    const Math::Vec3& b = local.maximum;
    const std::array<Math::Vec3, 8> corners{{{a.x, a.y, a.z}, {b.x, a.y, a.z}, {a.x, b.y, a.z}, {b.x, b.y, a.z},
                                             {a.x, a.y, b.z}, {b.x, a.y, b.z}, {a.x, b.y, b.z}, {b.x, b.y, b.z}}};
    Math::Vec3 low = Math::TransformPoint(world, corners[0]);
    Math::Vec3 high = low;
    for (const Math::Vec3& corner : corners) {
        const Math::Vec3 p = Math::TransformPoint(world, corner);
        low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
        high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
    }
    return {low, high};
}

bool InRange(const Render::VisibilityRange& range, const float distance) noexcept {
    return distance >= range.startMargin.start && distance < range.endMargin.end;
}

bool Sees(const Math::Frustum& frustum, const Math::Vec3& cameraPosition, const Candidate& candidate) noexcept {
    if (candidate.aabb != nullptr && !candidate.noFrustumCulling &&
        !Math::Intersects(frustum, WorldBox(candidate.aabb->local, candidate.world))) {
        return false;
    }
    if (candidate.range == nullptr) return true;
    Math::Vec3 position{candidate.world.values[12], candidate.world.values[13], candidate.world.values[14]};
    if (candidate.range->useAabb && candidate.aabb != nullptr) {
        const Math::Aabb& box = candidate.aabb->local;
        const Math::Vec3 centre{(box.minimum.x + box.maximum.x) * 0.5F, (box.minimum.y + box.maximum.y) * 0.5F,
                                (box.minimum.z + box.maximum.z) * 0.5F};
        position = Math::TransformPoint(candidate.world, centre);
    }
    const Math::Vec3 offset{position.x - cameraPosition.x, position.y - cameraPosition.y, position.z - cameraPosition.z};
    return InRange(*candidate.range, Math::Length(offset));
}

} // namespace GYO::Engine::Visibility
