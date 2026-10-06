#pragma once

#include "RetroFPS/Pvp/ShotQuery.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <vector>

namespace fps::pvp {

using ActionId = std::uint64_t;
inline constexpr std::size_t MaxActionBatch = 8;
inline constexpr std::size_t MaxActionWindow = 32;
inline constexpr std::size_t MaxPublishedShotReferences = 64;
inline constexpr unsigned ActionSendRate = 30;

// Product policy. Wire Ready/Welcome will use this same source in batch 03.
struct CombatRules final {
    std::uint32_t maximumHp{100};
    std::uint32_t shotDamage{25};
    std::uint64_t cooldownTicks{10};
    float shotRange{100};
    std::chrono::nanoseconds maximumReferenceAge{std::chrono::milliseconds{250}};
    std::uint32_t magazineCapacity{12};
    std::uint64_t reloadTicks{90};
    std::uint64_t respawnTicks{180};
};
inline constexpr CombatRules PvpCombatRules{};

enum class ActionKind { Shot = 0, Reload = 1 };

// Historical Shot names also carry reload actions.
struct ShotRequest final {
    ActionId actionId{};
    std::uint64_t observedAuthorityTick{};
    float yaw{};
    float pitch{};
    ActionKind kind{ActionKind::Shot};
    std::uint64_t lifeGeneration{1};
    bool operator==(const ShotRequest&) const = default;
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
    // Presentation-only hit record of this life (pv6 contract §2): the tick of
    // the last hit, hits taken and the last attacker. All zero before the first
    // hit; a respawn resets them with the rest of the state. Match never reads them.
    std::uint64_t lastDamageTick{};
    std::uint32_t damageCount{};
    PlayerId lastAttackerId{};
    bool operator==(const CombatState&) const = default;
};

// Decode check of the hit record (pv6 contract §2): all zero or all set; a hit
// lies within the current life state and the snapshot, is not self-inflicted
// and counts at most one per hit point; a dead player died of its last hit.
[[nodiscard]] constexpr bool ValidDamageRecord(const CombatState& combat, bool dead, std::uint64_t lifeStateTick,
                                               std::uint64_t snapshotTick, std::uint32_t maximumHp) noexcept {
    const bool none = combat.lastDamageTick == 0;
    if ((combat.damageCount == 0) != none || (combat.lastAttackerId == 0) != none) return false;
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
