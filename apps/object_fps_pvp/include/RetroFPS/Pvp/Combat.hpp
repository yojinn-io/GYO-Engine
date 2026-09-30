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

// Historical Shot names also carry reload actions in protocol v5.
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
    bool operator==(const CombatState&) const = default;
};

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
