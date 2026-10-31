#pragma once

#include "RetroFPS/Pvp/ShotQuery.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace fps::pvp {

using ActionId = std::uint64_t;
inline constexpr std::size_t MaxActionBatch = 8;
inline constexpr std::size_t MaxActionWindow = 32;
inline constexpr std::size_t MaxPublishedShotReferences = 64;
inline constexpr unsigned ActionSendRate = 30;

// Product policy. Wire Ready/Welcome will use this same source in batch 03.
// HP and damage are in milli-points (pv9 §3): 100 HP is 100000.
struct CombatRules final {
    std::uint32_t maximumHp{100000};
    std::uint32_t shotDamage{25000};
    std::uint64_t cooldownTicks{10};
    float shotRange{100};
    std::chrono::nanoseconds maximumReferenceAge{std::chrono::milliseconds{250}};
    std::uint32_t magazineCapacity{12};
    std::uint64_t reloadTicks{90};
    std::uint64_t respawnTicks{180};
    // The most damage one decision may deal, part multipliers included (pv7
    // §7, pv9 §3); shotDamage stays the base (Body) damage.
    std::uint32_t maximumShotDamage{50000};
    // Part multipliers in permille of shotDamage (pv9 §3). Match carries them
    // only; the wire carries maximumShotDamage. Leg reaches the wire in pv9's
    // leg batch.
    std::uint32_t headPermille{2000};
    std::uint32_t bodyPermille{1000};
    std::uint32_t legPermille{750};
};
inline constexpr CombatRules PvpCombatRules{};

// Whole points a Client shows for milli-points (pv9 §3): rounded up, so a
// living player (hp >= 1) never reads 0.
[[nodiscard]] constexpr std::uint32_t DisplayPoints(std::uint32_t milliPoints) noexcept {
    return milliPoints / 1000 + (milliPoints % 1000 != 0 ? 1U : 0U);
}

// A1 (pv7 §6): W, the furthest step a request may claim ahead of the
// shooter's last resolved command, and the cooldown hold bound. K follows the
// weapon's cooldown, so it never exceeds it whatever the rules say (v7.1 D136,
// network-architecture §7).
inline constexpr std::uint64_t MaxShotStepLead = MaxPendingCommands + 1;
inline constexpr std::uint64_t MaxShotHoldTicks = 2;
[[nodiscard]] constexpr std::uint64_t ShotHoldLimit(std::uint64_t cooldownTicks) noexcept {
    return cooldownTicks < MaxShotHoldTicks ? cooldownTicks : MaxShotHoldTicks;
}

enum class ActionKind { Shot = 0, Reload = 1 };
// Where a shot hit a player (pv7 §7, pv9 §2). Match produces only Body until
// the hitbox batch judges parts.
enum class HitPart : std::uint8_t { Unspecified = 0, Body = 1, Head = 2, Leg = 3 };
[[nodiscard]] constexpr bool PlayerHitPart(HitPart part) noexcept {
    return part == HitPart::Body || part == HitPart::Head || part == HitPart::Leg;
}

// Historical Shot names also carry reload actions.
struct ShotRequest final {
    ActionId actionId{};
    std::uint64_t observedAuthorityTick{};
    float yaw{};
    float pitch{};
    ActionKind kind{ActionKind::Shot};
    std::uint64_t lifeGeneration{1};
    // Immutable request content (pv7 §6, §7): it takes part in comparing a
    // retransmission. E and S of one Client simulation step decide when the
    // authority judges the action (A1); the remote time the shooter saw (Tick x
    // 65536; 0 without one) stays a recorded diagnostic.
    std::uint64_t movementEpoch{};
    std::uint64_t commandSequence{};
    std::uint64_t viewTickQ16{};
    bool operator==(const ShotRequest&) const = default;
};

// The request content a Client takes from its presentation for a shot (pv7 §6,
// §7): E and S of one simulation step, and the global view cursor (Tick x
// 65536). All zero without prediction or without a presented remote player.
struct ShotContext final {
    std::uint64_t movementEpoch{};
    std::uint64_t commandSequence{};
    std::uint64_t viewTickQ16{};
};

struct ActionBatch final {
    PlayerId playerId{};
    std::vector<ShotRequest> shots;
};

// Admission failure is retryable with the original IDs, never a shot decision.
enum class ActionAdmission { Accepted, InvalidPlayer, InvalidBatch, Conflict, OutsideWindow, Full };
enum class ShotRejection {
    None, InvalidReference, Expired, Cooldown,
    Dead, StaleLife, InvalidLife, Reloading, EmptyMagazine, MagazineFull
};

inline constexpr std::size_t ShotRejectionCount = static_cast<std::size_t>(ShotRejection::MagazineFull) + 1;

// The shared spelling of a rejection in logs and records (match-shots.jsonl,
// the Client's PvP summary).
[[nodiscard]] inline std::string_view ShotRejectionName(ShotRejection rejection) noexcept {
    switch (rejection) {
    case ShotRejection::None: return "none";
    case ShotRejection::InvalidReference: return "invalid_reference";
    case ShotRejection::Expired: return "expired";
    case ShotRejection::Cooldown: return "cooldown";
    case ShotRejection::Dead: return "dead";
    case ShotRejection::StaleLife: return "stale_life";
    case ShotRejection::InvalidLife: return "invalid_life";
    case ShotRejection::Reloading: return "reloading";
    case ShotRejection::EmptyMagazine: return "empty_magazine";
    case ShotRejection::MagazineFull: return "magazine_full";
    }
    return "unknown";
}

struct ShotDecision final {
    ActionId actionId{};
    std::uint64_t resolvedTick{};
    bool accepted{};
    ShotRejection rejection{ShotRejection::None};
    ShotHitKind hitKind{ShotHitKind::Miss};
    PlayerId targetId{};
    std::uint32_t damage{};
    ActionKind kind{ActionKind::Shot};
    std::uint64_t lifeGeneration{1};
    std::uint64_t targetLifeGeneration{};
    HitPart hitPart{HitPart::Unspecified}; // Body, Head or Leg exactly for a player hit
    bool operator==(const ShotDecision&) const = default;
};

struct CombatState final {
    PlayerId playerId{};
    std::uint32_t hp{PvpCombatRules.maximumHp};
    std::uint64_t nextAllowedShotTick{};
    std::uint64_t lifeGeneration{1};
    std::uint32_t magazineAmmo{PvpCombatRules.magazineCapacity};
    ActionId reloadActionId{};
    std::uint64_t reloadStartTick{};
    std::uint64_t reloadEndTick{};
    ActionId lastShotActionId{};
    std::uint64_t lastShotTick{};
    // Hit record of this life (pv6 contract §2, pv7 §7): the tick of the last
    // hit, hits taken, the last attacker and the part it hit. All zero before
    // the first hit; a respawn resets them with the rest of the state. From
    // pv9 the hit, shot and reload records select the upper body of the judged
    // pose (pv9-upper-body-in-hitbox (a), protocol-v9 §7): decision inputs the
    // Match reads from the hitbox batch on, no longer presentation only.
    std::uint64_t lastDamageTick{};
    std::uint32_t damageCount{};
    PlayerId lastAttackerId{};
    HitPart lastHitPart{HitPart::Unspecified};
    bool operator==(const CombatState&) const = default;
};

// pv7 §7 decode checks shared by every reader of the wire values: a player
// hit carries Body, Head or Leg (pv9 §2), any other decision Unspecified; and
// shot damage <= maximum shot damage <= maximum hp.
[[nodiscard]] constexpr bool ValidDecisionHitPart(ShotHitKind kind, int part) noexcept {
    const bool playerPart = part == static_cast<int>(HitPart::Body) || part == static_cast<int>(HitPart::Head) ||
                            part == static_cast<int>(HitPart::Leg);
    return kind == ShotHitKind::Player ? playerPart : part == static_cast<int>(HitPart::Unspecified);
}
[[nodiscard]] constexpr bool ValidShotDamageBounds(std::uint32_t shotDamage, std::uint32_t maximumShotDamage,
                                                   std::uint32_t maximumHp) noexcept {
    return shotDamage > 0 && shotDamage <= maximumShotDamage && maximumShotDamage <= maximumHp;
}

// Damage of one part before the target's hp caps it (pv9 §3): shotDamage x
// permille / 1000, the product in 64 bits.
[[nodiscard]] constexpr std::uint64_t PartDamage(std::uint32_t shotDamage, std::uint32_t permille) noexcept {
    return static_cast<std::uint64_t>(shotDamage) * permille / 1000;
}
// Match's rule check of one part: the product divides exactly, and the damage
// is at least 1 and at most maximumShotDamage.
[[nodiscard]] constexpr bool ValidPartDamage(std::uint32_t shotDamage, std::uint32_t permille,
                                             std::uint32_t maximumShotDamage) noexcept {
    const auto damage = PartDamage(shotDamage, permille);
    return static_cast<std::uint64_t>(shotDamage) * permille % 1000 == 0 && damage >= 1 && damage <= maximumShotDamage;
}
[[nodiscard]] constexpr bool ValidPartDamage(const CombatRules& rules) noexcept {
    return ValidPartDamage(rules.shotDamage, rules.headPermille, rules.maximumShotDamage) &&
           ValidPartDamage(rules.shotDamage, rules.bodyPermille, rules.maximumShotDamage) &&
           ValidPartDamage(rules.shotDamage, rules.legPermille, rules.maximumShotDamage);
}
// The damage one hit on a part deals to a target with hp left (pv9 §3). The
// rules passed ValidPartDamage, so the part damage fits 32 bits.
[[nodiscard]] constexpr std::uint32_t ShotDamage(const CombatRules& rules, HitPart part, std::uint32_t hp) noexcept {
    const auto permille = part == HitPart::Head ? rules.headPermille
                        : part == HitPart::Leg  ? rules.legPermille
                                                : rules.bodyPermille;
    const auto damage = PartDamage(rules.shotDamage, permille);
    return damage < hp ? static_cast<std::uint32_t>(damage) : hp;
}

// Decode check of the hit record (pv6 contract §2, four fields in pv7 §7):
// all zero or all set, a set part being Body, Head or Leg; a hit lies within the
// current life state and the snapshot, is not self-inflicted and counts at
// most one per hit point; a dead player died of its last hit.
[[nodiscard]] constexpr bool ValidDamageRecord(const CombatState& combat, bool dead, std::uint64_t lifeStateTick,
                                               std::uint64_t snapshotTick, std::uint32_t maximumHp) noexcept {
    const bool none = combat.lastDamageTick == 0;
    if ((combat.damageCount == 0) != none || (combat.lastAttackerId == 0) != none) return false;
    if ((combat.lastHitPart == HitPart::Unspecified) != none ||
        (!none && !PlayerHitPart(combat.lastHitPart))) return false;
    if (dead && (none || combat.lastDamageTick != lifeStateTick)) return false;
    return none || (lifeStateTick <= combat.lastDamageTick && combat.lastDamageTick <= snapshotTick &&
                    combat.lastAttackerId != combat.playerId && combat.damageCount <= maximumHp);
}

struct ActionResults final {
    PlayerId playerId{};
    ActionId retiredThrough{};
    // Owning, non-destructive view of all unacknowledged terminal decisions.
    std::vector<ShotDecision> decisions;
};

// Trusted host metadata, evaluated at resolution. Nullopt means unpublished,
// evicted or otherwise invalid. The domain never reads a clock itself.
using ShotReferenceAge = std::function<std::optional<std::chrono::nanoseconds>(std::uint64_t)>;

} // namespace fps::pvp
