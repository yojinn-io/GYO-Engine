#include "RetroFPS/Gameplay/Enemy/EnemyRig.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec3.hpp"
#include <stdexcept>

namespace fps {
Engine::Math::Vec3 EnemyBoneWorldPoint(const EnemyRig& rig, const Engine::Model::Pose& pose,
                           const EnemyBonePoint& point, GroundPoint position, float yaw) {
    if (point.node >= pose.globalTransforms.size())
        throw std::invalid_argument("Enemy bone is outside its authoritative pose");
    const auto p = Engine::Math::TransformPoint(pose.globalTransforms[point.node], point.offset);
    // Where the renderer draws the bone: the instance offsets vertices by
    // -anchor, then the renderer applies ComposeEulerXYZ (EnemyPresentation).
    return Engine::Math::TransformPoint(
        Engine::Math::ComposeEulerXYZ({position.x, 0.0F, position.z}, {0.0F, yaw, 0.0F},
                                      {rig.scale, rig.scale, rig.scale}),
        p - rig.anchor);
}
std::vector<EnemyHurtbox> BuildEnemyHurtboxes(const EnemyRig& rig, const Engine::Model::Pose& pose,
                                              GroundPoint position, float yaw) {
    std::vector<EnemyHurtbox> result;
    result.reserve(rig.hurtRegions.size());
    for (const auto& region : rig.hurtRegions) {
        auto a = EnemyBoneWorldPoint(rig, pose, region.start, position, yaw);
        auto b = EnemyBoneWorldPoint(rig, pose, region.end, position, yaw);
        result.push_back(
            {region.id, {a, b, region.radius * rig.scale}});
    }
    return result;
}
Engine::Model::Pose BuildEnemyWeaponPose(const EnemyRig& rig, const Engine::Model::Pose& pose) {
    if (!rig.weapon || !rig.weapon->model)
        throw std::invalid_argument("Enemy has no resolved weapon attachment");
    const auto& weapon = *rig.weapon;
    if (weapon.node >= pose.globalTransforms.size())
        throw std::invalid_argument("Enemy weapon bone is outside its authoritative pose");
    Engine::Model::Pose result;
    const auto made = Engine::Model::MakeDefaultPose(*weapon.model, result);
    if (!made)
        throw std::invalid_argument("Enemy weapon pose: " + made.error().message);
    const auto mount = Engine::Math::Multiply(pose.globalTransforms[weapon.node],
                                               Engine::Model::ToMatrix(weapon.localTransform));
    for (auto& transform : result.globalTransforms)
        transform = Engine::Math::Multiply(mount, transform);
    return result;
}
} // namespace fps
