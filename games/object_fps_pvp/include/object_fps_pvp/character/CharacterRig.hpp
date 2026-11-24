#pragma once

// The player's character as the Match and the Client both use it: the body model's skeleton (Player_V20.fbx), its
// sixteen clips retargeted from the animation library (UAL1_Standard.fbx, by node name, characters/player/
// character.json), the upper body (presentation.json's upper_body_root and below), how the model stands in the
// world (scaled to the arena's body height, its lowest point on the feet, turned by the yaw), the hit shapes on its
// bones (hitbox.json: port inventory PV2) and the third-person weapon's mount. The models are the catalog entries the
// character files name (asset_catalog.json, with their settings: the library without meshes). Both ends read them
// before the World is built, without any renderer (port inventory 4.5); a Client draws the same body.
//
// The model library does the sampling, the blending, the retargeting and the model-space matrices (layer 1); which
// clip plays and when is the game's (sim/PlayerAnimation.hpp, layer 2); the layering of the upper body over the
// lower is the game's too (port inventory 4.3).

#include "object_fps_pvp/sim/CombatRules.hpp"
#include "object_fps_pvp/sim/PlayerAnimation.hpp"

#include "engine/math/linear/Matrix4.hpp"
#include "engine/model/ModelData.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace object_fps_pvp::character {

namespace Math = GYO::Engine::Math;
namespace Model = GYO::Engine::Model;

enum class Clip : std::uint8_t {
    Idle = 0, Walk, Jog, Shoot, Reload, JumpStart, JumpLoop, JumpLand, Death,
    AimUp, AimNeutral, AimDown, Hit, HitHead, CrouchIdle, CrouchMove, Count
};
inline constexpr std::size_t kClipCount = static_cast<std::size_t>(Clip::Count);

// A hit shape on the bones: a ball on one node (offset in that node's space), or a capsule between two nodes;
// radius in world units.
struct HitShapeDefinition final {
    sim::HitPart part = sim::HitPart::Body;
    bool ball = false;
    std::uint32_t node = 0;
    std::uint32_t nodeTo = 0;
    Math::Vec3 offset{};
    float radius = 0.0F;
};

// One placed hit shape in world space: a capsule from a to b (a ball has a == b).
struct HitShape final {
    sim::HitPart part = sim::HitPart::Body;
    Math::Vec3 a{};
    Math::Vec3 b{};
    float radius = 0.0F;
};

struct CharacterRig final {
    // The body model and its catalog id (a Client draws the store's copy of the same file: its node order is this one).
    std::shared_ptr<const Model::ModelData> body;
    std::string bodyAssetId;
    std::array<Model::AnimationClip, kClipCount> clips;
    std::vector<std::uint8_t> upperMask;  // per node: 1 for the upper body
    sim::AnimationRules rules;
    sim::PresentationFile presentation;
    // Model to world: world = translate(feet) x rotateY(yaw) x scale(scale) x translate(-anchor).
    float scale = 1.0F;
    Math::Vec3 anchor{};
    std::vector<HitShapeDefinition> hitShapes;
    std::uint32_t weaponNode = 0;
    Math::Matrix4 weaponMount{};
};

// Reads the character under root (the game's asset directory) for a body of bodyHeight. nullopt with error naming
// the file and what was wrong (a clip or node the files name that the models do not have, a retarget refused).
[[nodiscard]] std::optional<CharacterRig> LoadCharacterRig(const std::string& root, float bodyHeight, std::string& error);

// The body's local pose for the moment: the lower body from the locomotion (or the jump, or the death), the upper
// body from the aim plus the action's (or the idle's) motion since its first frame, each node from its half. out takes
// one transform per node.
void ComposeBody(const CharacterRig& rig, const sim::Locomotion& locomotion, const sim::ActionPose& actions, bool crouched,
                 double idleSeconds, std::vector<Model::LocalTransform>& out);

// Every node's model-space matrix of a local pose.
void ModelSpace(const CharacterRig& rig, const std::vector<Model::LocalTransform>& local, std::vector<Math::Matrix4>& out);

// The model's placement in the world for a player standing at feet and looking at yaw.
[[nodiscard]] Math::Matrix4 BodyToWorld(const CharacterRig& rig, const Math::Vec3& feet, float yaw);

// The hit shapes of a pose (model-space matrices) in the world.
[[nodiscard]] std::vector<HitShape> PlaceHitShapes(const CharacterRig& rig, const std::vector<Math::Matrix4>& modelSpace,
                                                   const Math::Vec3& feet, float yaw);

// The third-person weapon's world matrix (the mount on its node, times the body's placement).
[[nodiscard]] Math::Matrix4 WeaponToWorld(const CharacterRig& rig, const std::vector<Math::Matrix4>& modelSpace,
                                          const Math::Vec3& feet, float yaw);

} // namespace object_fps_pvp::character
