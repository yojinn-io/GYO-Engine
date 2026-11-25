#pragma once

// The game's data in a World: the player's components (decision data under the server authority, replicated from
// the Match), the input component, and the shared rules. Every rule that changes these runs in the fixed step
// (world/PvpWorld.hpp); the Client predicts the movement and the weapon of its own player and takes the rest from
// the Match.

#include "object_fps_pvp/sim/Arena.hpp"
#include "object_fps_pvp/sim/CombatRules.hpp"
#include "object_fps_pvp/sim/LifeState.hpp"
#include "object_fps_pvp/sim/MovementRules.hpp"
#include "object_fps_pvp/sim/PlayerAnimation.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <bit>
#include <cstdint>
#include <memory>
#include <string>

namespace object_fps_pvp::character {
struct CharacterRig;
}

namespace object_fps_pvp::world {

namespace K = GYO::Kernel;
namespace Math = GYO::Engine::Math;

// Where a player stands and looks. feet is the bottom of its upright capsule; the yaw turns +Z toward +X and a
// positive pitch looks down.
struct PlayerBody final {
    Math::Vec3 feet{};
    float verticalVelocity = 0.0F;
    float yaw = 0.0F;
    float pitch = 0.0F;
    std::uint8_t grounded = 1;
    std::uint8_t crouched = 0;
    // Bit by bit (each float compares its bits), the same equality HashValue hashes: a rollback compares with it.
    friend bool operator==(const PlayerBody& a, const PlayerBody& b) noexcept;
};

inline bool operator==(const PlayerBody& a, const PlayerBody& b) noexcept {
    const auto same = [](const float x, const float y) { return std::bit_cast<std::uint32_t>(x) == std::bit_cast<std::uint32_t>(y); };
    return same(a.feet.x, b.feet.x) && same(a.feet.y, b.feet.y) && same(a.feet.z, b.feet.z) &&
           same(a.verticalVelocity, b.verticalVelocity) && same(a.yaw, b.yaw) && same(a.pitch, b.pitch) &&
           a.grounded == b.grounded && a.crouched == b.crouched;
}

// A player's life and weapon. Ticks are the World's fixed steps; reloadEndTick 0: not reloading.
struct PlayerCombat final {
    std::uint32_t hp = 0;
    std::uint32_t ammo = 0;
    sim::LifeState life = sim::LifeState::Alive;
    // Lives begun so far (1 for the first): a respawn adds one.
    std::uint32_t lives = 1;
    // The tick this life (or this death) began.
    std::uint64_t lifeTick = 0;
    std::uint64_t nextShotTick = 0;
    std::uint64_t reloadEndTick = 0;
    std::uint64_t respawnTick = 0;
    // The tick of the last shot this player fired (0: none yet) and the shots fired in all.
    std::uint64_t lastShotTick = 0;
    std::uint32_t shots = 0;
    friend bool operator==(const PlayerCombat&, const PlayerCombat&) = default;
};

// The Match's judgement of a player's last shot (written only at the Match).
struct ShotRecord final {
    std::uint64_t tick = 0;
    sim::ShotHitKind kind = sim::ShotHitKind::Miss;
    K::EntityId target = 0;
    sim::HitPart part = sim::HitPart::None;
    std::uint32_t damage = 0;
    float distance = 0.0F;
    friend bool operator==(const ShotRecord&, const ShotRecord&) = default;
};

// The hits a player took in this life: the tick of the last, how many, who and where (written only at the Match;
// the hit feedback and the hit sounds read it).
struct HitRecord final {
    std::uint64_t tick = 0;
    std::uint32_t hits = 0;
    K::EntityId attacker = 0;
    sim::HitPart part = sim::HitPart::None;
    friend bool operator==(const HitRecord&, const HitRecord&) = default;
};

// How a player's animation stands at the Match (its gait, its jump): what the hit shapes on its bones are placed
// from (port inventory PV2). Not decision data and not replicated (port declaration PV6): only the Match keeps it,
// stepped by AnimatePlayers; a Client keeps its own for the picture.
struct PlayerMotion final {
    sim::Locomotion locomotion;
};

// Whose player this is: the user id the Client connected with.
struct Owner final {
    std::uint64_t userId = 0;
    friend bool operator==(const Owner&, const Owner&) = default;
};

// One step of a player's input. present 0: no input this step (a player the Client does not control, or before
// the first input reached the Match): the player neither moves nor turns. jump, fire and reload are presses (one
// step each; the net Module clears them when it holds the last input); crouch is held.
struct PlayerInput final {
    float forward = 0.0F;
    float right = 0.0F;
    float yaw = 0.0F;
    float pitch = 0.0F;
    std::uint8_t present = 0;
    std::uint8_t jump = 0;
    std::uint8_t crouch = 0;
    std::uint8_t fire = 0;
    std::uint8_t reload = 0;
    // The Client has loaded the game and drawn it (held: every input after says so); the Match starts when every
    // expected player is ready (game-flow declaration 3.5).
    std::uint8_t ready = 0;
    friend bool operator==(const PlayerInput&, const PlayerInput&) = default;
};

void HashValue(K::Hasher& h, const PlayerBody& v);
void HashValue(K::Hasher& h, const PlayerCombat& v);
void HashValue(K::Hasher& h, const ShotRecord& v);
void HashValue(K::Hasher& h, const HitRecord& v);
void HashValue(K::Hasher& h, const Owner& v);
void HashValue(K::Hasher& h, const PlayerMotion& v);
void HashValue(K::Hasher& h, const PlayerInput& v);

// The match's rules and arena, the same at the Match and every Client (MatchInfo tells the Client which arena).
struct GameRules final {
    sim::ArenaContent arena;
    sim::StanceRules stance;
    sim::CombatRules combat;
    // The arena's walls (BoxSet::Digest): a Client compares it with the Match's (MatchInfo).
    std::uint64_t wallsDigest = 0;
    // The character whose bones carry the hit shapes (LoadGameContent always gives one).
    std::shared_ptr<const character::CharacterRig> character;
};
void HashValue(K::Hasher&, const GameRules&);

// Before the start the players wait where they stand and see only themselves; then the match is played (game-flow
// declaration 3.5).
enum class MatchPhase : std::uint8_t { Waiting = 0, Playing = 1 };

// The Match tells every Client which arena it plays and the digest of its walls (replicated; port declaration PV14),
// and whether the match has started.
struct MatchInfo final {
    std::string arenaId;
    std::uint64_t wallsDigest = 0;
    MatchPhase phase = MatchPhase::Waiting;
    friend bool operator==(const MatchInfo&, const MatchInfo&) = default;
};
void HashValue(K::Hasher&, const MatchInfo&);

// A Client's own user id (the one it connected with): the player whose Owner has it is the one it controls.
struct LocalUser final {
    std::uint64_t userId = 0;
};
void HashValue(K::Hasher&, const LocalUser&);

// The entity of the arena's walls (Collider + Transform), put at both ends with this id, outside every allotter
// range (WorldOptions::serverIds and clientIds), before the first step.
inline constexpr K::EntityId kArenaEntity = 3'000'001;

} // namespace object_fps_pvp::world
