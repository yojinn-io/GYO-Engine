#include "RetroFPS/Gameplay/Enemy/EnemyRig.hpp"
#include <cmath>
#include <stdexcept>

namespace fps {
Float3 EnemyBoneWorldPoint(const EnemyRig& rig, const Engine::Model::Pose& pose,
                           const EnemyBonePoint& point, Float2 position, float yaw) {
    if (point.node >= pose.globalTransforms.size())
        throw std::invalid_argument("Enemy bone is outside its authoritative pose");
    const auto p = Engine::Model::TransformPoint(pose.globalTransforms[point.node], point.offset);
    const float x = (p.x - rig.anchor.x) * rig.scale, z = (p.z - rig.anchor.z) * rig.scale;
    return {position.x + std::cos(yaw) * x + std::sin(yaw) * z, (p.y - rig.anchor.y) * rig.scale,
            position.z - std::sin(yaw) * x + std::cos(yaw) * z};
}
std::vector<EnemyHurtbox> BuildEnemyHurtboxes(const EnemyRig& rig, const Engine::Model::Pose& pose,
                                              Float2 position, float yaw) {
    std::vector<EnemyHurtbox> result;
    result.reserve(rig.hurtRegions.size());
    for (const auto& region : rig.hurtRegions) {
        auto a = EnemyBoneWorldPoint(rig, pose, region.start, position, yaw);
        auto b = EnemyBoneWorldPoint(rig, pose, region.end, position, yaw);
        result.push_back(
            {region.id, {{a.x, a.y, a.z}, {b.x, b.y, b.z}, region.radius * rig.scale}});
    }
    return result;
}
} // namespace fps
