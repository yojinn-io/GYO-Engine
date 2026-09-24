#pragma once

#include "RetroFPS/Collision/GridCollision.hpp"
#include "RetroFPS/Data/GameData.hpp"
#include "RetroFPS/Gameplay/Enemy/EnemyRig.hpp"
#include "RetroFPS/Math/Vector.hpp"
#include "RetroFPS/World/GridMap.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fps {

using EnemyId = std::uint64_t;

inline constexpr float kEnemyHitFlashSeconds = 0.12f;

enum class EnemyState : std::uint8_t {
    Idle,
    Moving,
    Attacking,
    Dead,
};

struct EnemySettings final {
    float repathIntervalSeconds = 0.20f;

    float meleeSpeed = 1.8f;
    float meleeAttackSurfaceDistance = 0.15f;

    float rangedSpeed = 1.4f;
    float rangedTooCloseSurfaceDistance = 2.0f;
    float rangedIdealSurfaceDistance = 4.0f;
    float rangedMaximumAttackSurfaceDistance = 6.0f;
};

[[nodiscard]] bool ValidateEnemySettings(
    const EnemySettings& settings,
    std::string& error);

struct EnemySnapshot final {
    EnemyId id = 0;
    EnemyDefinitionId definitionId{};
    EnemyKind kind = EnemyKind::Melee;
    EnemyState state = EnemyState::Idle;
    Float2 position{};
    float collisionRadius = 0.0f;
    float hitboxHeight = 0.0f;
    float health = 0.0f;
    float maxHealth = 0.0f;
    float defense = 0.0f;
    float hitFlashRemainingSeconds = 0.0f;
    float stateElapsedSeconds = 0.0f;
    float yawRadians{};
    Engine::Model::Pose pose;
    Engine::Collision::VerticalCapsule body;
    std::vector<EnemyHurtbox> hurtboxes;
    std::optional<Engine::Collision::Capsule> attackShape;
};

struct EnemyAttackEvent final {
    EnemyId enemyId = 0;
    EnemyDefinitionId definitionId{};
    EnemyKind kind = EnemyKind::Melee;
    Float3 origin{};
    Float3 target{};
    float damage = 0.0f;
};

struct EnemyTarget final {
    Float2 position{};
    float collisionRadius = 0.0f;
    float hitboxHeight = 0.0f;
    float feetY = 0.0f;
};

enum class EnemySpawnStatus : std::uint8_t {
    Spawned,
    Blocked,
    Invalid,
};

struct EnemySpawnResult final {
    EnemySpawnStatus status = EnemySpawnStatus::Invalid;
    EnemyId enemyId = 0;

    [[nodiscard]] bool Spawned() const noexcept {
        return status == EnemySpawnStatus::Spawned;
    }
};

struct EnemyDamageResult final {
    bool applied = false;
    bool killed = false;
    float rawDamage = 0.0f;
    float appliedDamage = 0.0f;
    float remainingHealth = 0.0f;
};

// CPU-only simulation using Engine animation and geometry. The map remains owned by the caller;
// Initialize and Update therefore both receive the map used by the current
// level session.
class EnemySystem final {
public:
    EnemySystem() = default;

    // Definitions and spawn policy are supplied explicitly through Spawn.
    [[nodiscard]] bool Initialize(
        const GridMap& map,
        Float2 playerPosition,
        float playerCollisionRadius,
        float cellSize,
        EnemySettings settings,
        std::string& error,
        float wallHeight = 2.5f,
        float playerHitboxHeight = 1.8f);
    void Reset() noexcept;

    [[nodiscard]] EnemySpawnResult Spawn(
        const GridMap& map,
        Float2 playerPosition,
        float playerCollisionRadius,
        Float2 spawnPosition,
        const EnemyDefinition& definition,
        std::string& error);
    [[nodiscard]] bool Retire(EnemyId id) noexcept;
    [[nodiscard]] bool RetireDead(EnemyId id) noexcept;
    [[nodiscard]] std::size_t RetireExpiredDead() noexcept;

    void Update(
        const GridMap& map,
        const EnemyTarget& player,
        float deltaSeconds);

    [[nodiscard]] std::span<const EnemySnapshot> GetSnapshots() const noexcept {
        return snapshots_;
    }
    [[nodiscard]] std::span<const EnemyAttackEvent> GetAttackEvents() const noexcept {
        return attackEvents_;
    }
    [[nodiscard]] std::vector<CircleObstacle> CollectAliveColliders() const;
    [[nodiscard]] std::vector<Engine::Collision::VerticalCapsule> CollectAliveBodies() const;
    // Includes live instances and dead instances whose death animation remains
    // visible, so a wave spawner cannot reuse an occupied slot.
    [[nodiscard]] std::vector<CircleObstacle> CollectOccupiedColliders() const;

    // Applies max(1, raw damage * the rig's region multiplier - definition defense).
    // Empty region uses multiplier 1 for direct damage. Unknown regions/enemies,
    // dead enemies and invalid raw damage return an unapplied result.
    [[nodiscard]] EnemyDamageResult ApplyDamage(
        EnemyId id, float rawDamage, std::string_view region = {});
    [[nodiscard]] bool Kill(EnemyId id);

    [[nodiscard]] const EnemySettings& GetSettings() const noexcept { return settings_; }
    [[nodiscard]] float GetCellSize() const noexcept { return cellSize_; }
    [[nodiscard]] std::size_t GetAliveCount() const noexcept;
    [[nodiscard]] std::size_t GetInstanceCount() const noexcept { return enemies_.size(); }
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }

private:
    enum class NavigationPurpose : std::uint8_t {
        None,
        Chase,
        Retreat,
        FiringPosition,
    };

    struct RuntimeEnemy final {
        EnemyId id = 0;
        EnemyKind kind = EnemyKind::Melee;
        EnemyState state = EnemyState::Idle;
        Float2 position{};
        float yawRadians{};
        Engine::Model::AnimationInstance animation;
        std::optional<Engine::Collision::Capsule> attackShape;
        EnemyDefinition definition{};
        float health = 0.0f;
        float hitFlashRemainingSeconds = 0.0f;
        float stateElapsedSeconds = 0.0f;
        float attackCooldownSeconds = 0.0f;
        bool attackEventEmitted = false;
        float repathElapsedSeconds = 0.0f;
        float stuckElapsedSeconds = 0.0f;
        NavigationPurpose navigationPurpose = NavigationPurpose::None;
        std::vector<GridCoordinate> path;
        std::size_t nextWaypointIndex = 0;
        std::optional<GridCoordinate> lastPlayerCell;
    };

    [[nodiscard]] bool ValidateDefinition(
        const EnemyDefinition& definition,
        std::string& error) const;
    void SetState(RuntimeEnemy& enemy, EnemyState state);
    void MarkDead(RuntimeEnemy& enemy);
    void AdvanceAnimation(RuntimeEnemy& enemy, const EnemyTarget& player,
        std::span<const Engine::Collision::Aabb> walls, float deltaSeconds);
    void UpdateSnapshot(const RuntimeEnemy& enemy, EnemySnapshot& snapshot) const;
    void RefreshSnapshots();

    EnemySettings settings_{};
    float cellSize_ = 1.0f;
    float wallHeight_ = 2.5f;
    float playerHitboxHeight_ = 1.8f;
    std::size_t mapWidth_ = 0;
    std::size_t mapHeight_ = 0;
    EnemyId nextEnemyId_ = 1;
    bool initialized_ = false;
    std::vector<RuntimeEnemy> enemies_;
    std::vector<EnemySnapshot> snapshots_;
    std::vector<EnemyAttackEvent> attackEvents_;
};

} // namespace fps
