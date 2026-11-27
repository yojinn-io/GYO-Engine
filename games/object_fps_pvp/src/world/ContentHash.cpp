#include "object_fps_pvp/world/ContentHash.hpp"

#include "engine/base/Fnv1a.hpp"

#include <string>
#include <string_view>

namespace object_fps_pvp::sim {

void HashValue(GYO::Kernel::Hasher& h, const StanceRules& v) {
    HashValue(h, v.crouchBodyHeight);
    HashValue(h, v.crouchEyeHeight);
    HashValue(h, v.crouchSpeedScale);
    HashValue(h, v.sprintSpeedScale);
    HashValue(h, v.holdJump);
}

void HashValue(GYO::Kernel::Hasher& h, const CombatRules& v) {
    HashValue(h, v.maximumHp);
    HashValue(h, v.shotDamage);
    HashValue(h, v.maximumShotDamage);
    HashValue(h, v.headPermille);
    HashValue(h, v.bodyPermille);
    HashValue(h, v.legPermille);
    HashValue(h, v.shotRange);
    HashValue(h, v.magazineCapacity);
    HashValue(h, v.cooldownSeconds);
    HashValue(h, v.reloadSeconds);
    HashValue(h, v.respawnSeconds);
}

void HashValue(GYO::Kernel::Hasher& h, const AnimationRules& v) {
    HashValue(h, v.referenceSpeed);
    HashValue(h, v.walkNativeSpeed);
    HashValue(h, v.jogNativeSpeed);
    HashValue(h, v.walkCycleDistance);
    HashValue(h, v.jogCycleDistance);
    HashValue(h, v.transitionSeconds);
    HashValue(h, v.maxFrameDeltaSeconds);
    HashValue(h, v.shotSeconds);
    HashValue(h, v.jumpStartSeconds);
    HashValue(h, v.jumpLandSeconds);
    HashValue(h, v.hitSeconds);
    HashValue(h, v.aimUpRadians);
    HashValue(h, v.aimDownRadians);
}

} // namespace object_fps_pvp::sim

namespace object_fps_pvp::character {

namespace {
void HashVec3(GYO::Kernel::Hasher& h, const GYO::Engine::Math::Vec3& v) {
    HashValue(h, v.x);
    HashValue(h, v.y);
    HashValue(h, v.z);
}
} // namespace

void HashValue(GYO::Kernel::Hasher& h, const HitShapeDefinition& v) {
    HashValue(h, v.part);
    HashValue(h, v.ball);
    HashValue(h, v.node);
    HashValue(h, v.nodeTo);
    HashVec3(h, v.offset);
    HashValue(h, v.radius);
}

} // namespace object_fps_pvp::character

namespace object_fps_pvp::world {

namespace K = GYO::Kernel;
namespace Model = GYO::Engine::Model;
namespace Math = GYO::Engine::Math;

std::uint64_t Digest64(const K::Hasher& hasher) {
    const auto bytes = hasher.Data();
    return GYO::Engine::Base::Fnv1a64(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

namespace {

// The model's values the digest takes (the Engine's types; the hashing is the game's).
void HashVec3(K::Hasher& h, const Math::Vec3& v) {
    HashValue(h, v.x);
    HashValue(h, v.y);
    HashValue(h, v.z);
}

void HashQuaternion(K::Hasher& h, const Math::Quaternion& q) {
    HashValue(h, q.x);
    HashValue(h, q.y);
    HashValue(h, q.z);
    HashValue(h, q.w);
}

// A node of the skeleton: its name, parent and rest pose.
void HashNode(K::Hasher& h, const Model::Node& node) {
    HashValue(h, node.name);
    HashValue(h, node.parent);
    HashVec3(h, node.rest.translation);
    HashQuaternion(h, node.rest.rotation);
    HashVec3(h, node.rest.scale);
}

template <class T, class Each>
void HashKeys(K::Hasher& h, const Model::Keys<T>& keys, Each each) {
    HashValue(h, keys.times);
    HashValue(h, static_cast<std::uint64_t>(keys.values.size()));
    for (const T& value : keys.values) each(h, value);
}

// A clip: its name, length, node count and every key of every track.
void HashClip(K::Hasher& h, const Model::AnimationClip& clip) {
    HashValue(h, clip.name);
    HashValue(h, clip.duration);
    HashValue(h, clip.nodeCount);
    HashValue(h, static_cast<std::uint64_t>(clip.tracks.size()));
    for (const Model::Track& track : clip.tracks) {
        HashValue(h, track.node);
        HashKeys(h, track.translation, HashVec3);
        HashKeys(h, track.rotation, HashQuaternion);
        HashKeys(h, track.scale, HashVec3);
    }
}

} // namespace

std::uint64_t CharacterDigest(const character::CharacterRig& character) {
    K::Hasher h;
    HashValue(h, std::string("gyo.object_fps_pvp.character/1"));
    HashValue(h, character.bodyAssetId);
    HashValue(h, static_cast<std::uint64_t>(character.body->nodes.size()));
    for (const Model::Node& node : character.body->nodes) HashNode(h, node);
    HashValue(h, static_cast<std::uint64_t>(character.clips.size()));
    for (const Model::AnimationClip& clip : character.clips) HashClip(h, clip);
    HashValue(h, character.upperMask);
    HashValue(h, character.rules);
    HashValue(h, character.scale);
    HashVec3(h, character.anchor);
    HashValue(h, character.hitShapes);
    return Digest64(h);
}

} // namespace object_fps_pvp::world
