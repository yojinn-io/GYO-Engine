#pragma once

#include "RetroFPS/Math/Vector.hpp"
#include "engine/asset/AssetId.hpp"
#include "engine/collision/Collision.hpp"
#include "model/Animation.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace fps {
struct EnemyBonePoint final {
    std::size_t node{};
    Engine::Model::Vec3 offset{};
};
struct EnemyHurtRegion final {
    std::string id;
    EnemyBonePoint start, end;
    float radius{}; // authored model metres, before uniform calibration
};
// Resolved, immutable CPU contract. Asset decoding and material assembly remain in App.
struct EnemyRig final {
    Engine::Asset::AssetId characterAssetId;
    std::shared_ptr<const Engine::Model::ModelAsset> model;
    std::array<std::size_t, 4> clips{}; // idle, move, attack, dead
    Engine::Model::Vec3 anchor{};
    float scale{1};
    std::vector<EnemyHurtRegion> hurtRegions;
    EnemyBonePoint attackPoint;
    float attackRadius{0.12F};
    double attackBeginSeconds{}, attackEndSeconds{}, releaseSeconds{};
    float transitionSeconds{0.10F};
};
struct EnemyHurtbox final {
    std::string region;
    Engine::Collision::Capsule shape;
};
[[nodiscard]] Float3 EnemyBoneWorldPoint(const EnemyRig& rig, const Engine::Model::Pose& pose,
                                         const EnemyBonePoint& point, Float2 position, float yaw);
[[nodiscard]] std::vector<EnemyHurtbox> BuildEnemyHurtboxes(const EnemyRig& rig,
                                                            const Engine::Model::Pose& pose,
                                                            Float2 position, float yaw);
} // namespace fps
