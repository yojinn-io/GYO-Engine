#pragma once

#include "object_fps_pvp/sim/Arena.hpp"

#include "engine/math/linear/Vec3.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace object_fps_pvp::sim {

// Where a shot hit a player. None: no player was hit.
enum class HitPart : std::uint8_t { None = 0, Body = 1, Head = 2, Leg = 3 };

// The weapon and life rules. HP and damage are in milli-points (100 HP is 100000); durations are seconds, turned
// into steps of the World's fixed step by TicksFor (the Match's tick rate is the time Module's, not a number here).
struct CombatRules final {
    std::uint32_t maximumHp = 100'000;
    std::uint32_t shotDamage = 25'000;
    std::uint32_t maximumShotDamage = 50'000;
    // Part multipliers in permille of shotDamage.
    std::uint32_t headPermille = 2000;
    std::uint32_t bodyPermille = 1000;
    std::uint32_t legPermille = 750;
    float shotRange = 100.0F;
    std::uint32_t magazineCapacity = 12;
    double cooldownSeconds = 1.0 / 6.0;
    double reloadSeconds = 1.5;
    double respawnSeconds = 3.0;
};

// Whole points a player sees for milli-points: rounded up, so a living player (hp >= 1) never reads 0.
[[nodiscard]] constexpr std::uint32_t DisplayPoints(const std::uint32_t milliPoints) noexcept {
    return milliPoints / 1000 + (milliPoints % 1000 != 0 ? 1U : 0U);
}

// The damage of one hit on part: shotDamage x the part's permille / 1000, at most maximumShotDamage and at most
// what the target has left. A shot that hit no part deals none.
[[nodiscard]] std::uint32_t ShotDamage(const CombatRules& rules, HitPart part, std::uint32_t targetHp) noexcept;

// Steps of stepNanoseconds that cover seconds: rounded up, at least 1.
[[nodiscard]] std::uint64_t TicksFor(double seconds, std::uint64_t stepNanoseconds) noexcept;

// The unit direction a view looks at: yaw 0 looks at +Z and turns toward +X; a positive pitch looks down.
[[nodiscard]] GYO::Engine::Math::Vec3 ViewDirection(float yaw, float pitch) noexcept;

// At one distance, the higher part of a target counts: Head over Body over Leg.
[[nodiscard]] int PartRank(HitPart part) noexcept;

// What a shot met first.
enum class ShotHitKind : std::uint8_t { Miss = 0, World = 1, Player = 2 };

// One player's candidate hit on a shot's ray.
struct PlayerHitCandidate final {
    std::uint64_t player = 0;
    float distance = 0.0F;
    HitPart part = HitPart::None;
};

struct ShotOutcome final {
    ShotHitKind kind = ShotHitKind::Miss;
    std::uint64_t player = 0;
    float distance = 0.0F;
    HitPart part = HitPart::None;
};

// The shot's outcome from the nearest world hit (nullopt: none within range) and each player's nearest hit: the
// nearest wins; a world hit at the same distance as a player wins; between players at one distance the lowest id.
[[nodiscard]] ShotOutcome ChooseShotOutcome(std::optional<float> world, std::span<const PlayerHitCandidate> players,
                                            float range) noexcept;

// The spawn to (re)appear at: among the spawns where a body can be placed (placeable), the one whose nearest living
// enemy is the farthest (squared distances of the float differences, in double); equal distances keep the content
// order, and with no enemy the first placeable spawn. nullopt when no spawn can be placed (the caller tries again
// on a later step).
[[nodiscard]] std::optional<std::size_t> ChooseSpawn(std::span<const SpawnPoint> spawns,
                                                     std::span<const GYO::Engine::Math::Vec3> enemies,
                                                     const std::function<bool(const SpawnPoint&)>& placeable);

} // namespace object_fps_pvp::sim
