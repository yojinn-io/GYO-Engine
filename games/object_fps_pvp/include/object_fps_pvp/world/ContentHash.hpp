#pragma once

// What the content comparisons hash (MatchInfo's rules, arena and character digests; mode-config declaration 3.3,
// 11.7; arena-rules-split declaration 3.6): each type's values through the Kernel's Hasher, never a file's bytes. The
// sim and character libraries have no Kernel, so their types' HashValue is here, found by argument-dependent lookup
// wherever this header is included.

#include "object_fps_pvp/character/CharacterRig.hpp"
#include "object_fps_pvp/sim/Arena.hpp"
#include "object_fps_pvp/sim/BodySize.hpp"
#include "object_fps_pvp/sim/CombatRules.hpp"
#include "object_fps_pvp/sim/DamageRules.hpp"
#include "object_fps_pvp/sim/MovementRules.hpp"
#include "object_fps_pvp/sim/PlayerAnimation.hpp"

#include "engine/kernel/Hash.hpp"

#include <cstdint>

namespace object_fps_pvp::sim {
void HashValue(GYO::Kernel::Hasher& h, const MovementRules& v);
void HashValue(GYO::Kernel::Hasher& h, const DamageRules& v);
void HashValue(GYO::Kernel::Hasher& h, const BodySize& v);
void HashValue(GYO::Kernel::Hasher& h, const SpawnPoint& v);
// An arena's own values (MatchInfo::arenaDigest): its map, bounds, floor, kill height and spawns. Not its id (compared
// on its own) nor its scene (the scene's content is compared on its own).
void HashValue(GYO::Kernel::Hasher& h, const ArenaContent& v);
void HashValue(GYO::Kernel::Hasher& h, const CombatRules& v);
void HashValue(GYO::Kernel::Hasher& h, const AnimationRules& v);
} // namespace object_fps_pvp::sim

namespace object_fps_pvp::character {
void HashValue(GYO::Kernel::Hasher& h, const HitShapeDefinition& v);
} // namespace object_fps_pvp::character

namespace object_fps_pvp::world {

// A Hasher's bytes as the 64-bit digest the comparisons carry.
[[nodiscard]] std::uint64_t Digest64(const GYO::Kernel::Hasher& hasher);

// The character's digest (the user's decision U1, mode-config declaration 11.7): what the Match judges the shots on
// and a Client predicts and draws with -- the body model's catalog id, the body (its size), its skeleton (each node's
// name, parent and rest pose), the clips as retargeted onto it (every key), the upper body's nodes, the animation
// rules, the scale and anchor to the world, and the hit shapes on the bones (hitbox.json). Left out: the meshes' vertices and the materials, and the
// third-person weapon's mount (the picture, not the play).
[[nodiscard]] std::uint64_t CharacterDigest(const character::CharacterRig& character);

} // namespace object_fps_pvp::world
