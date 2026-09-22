#include "RetroFPS/Gameplay/Enemy/EnemySystem.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fps {
namespace {

constexpr float kPositionEpsilon = 0.0001f;
constexpr float kWaypointTolerance = 0.025f;

[[nodiscard]] bool IsFinite(const Float2 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.z);
}

[[nodiscard]] float ClipDurationSeconds(
    const EnemyDefinition& definition, const EnemyState state) noexcept {
    const double duration = definition.rig->model->clips[
        definition.rig->clips[static_cast<std::size_t>(state)]].durationSeconds;
    return static_cast<float>((std::min)(
        duration, static_cast<double>((std::numeric_limits<float>::max)())));
}

[[nodiscard]] float DeadVisibilitySeconds(
    const EnemyDefinition& definition) noexcept {
    return (std::max)(
        kEnemyHitFlashSeconds,
        ClipDurationSeconds(definition, EnemyState::Dead));
}

[[nodiscard]] float Distance(const Float2 left, const Float2 right) noexcept {
    const double deltaX = static_cast<double>(right.x) - left.x;
    const double deltaZ = static_cast<double>(right.z) - left.z;
    const double distance = std::hypot(deltaX, deltaZ);
    return static_cast<float>((std::min)(
        distance, static_cast<double>((std::numeric_limits<float>::max)())));
}

[[nodiscard]] float SurfaceDistance(
    const Float2 left,
    const float leftRadius,
    const Float2 right,
    const float rightRadius) noexcept {
    return Distance(left, right) - leftRadius - rightRadius;
}

void AddElapsed(float& elapsedSeconds, const float deltaSeconds) noexcept {
    const double sum = static_cast<double>(elapsedSeconds) + deltaSeconds;
    elapsedSeconds = static_cast<float>((std::min)(
        sum, static_cast<double>((std::numeric_limits<float>::max)())));
}

void SubtractElapsed(float& remainingSeconds, const float deltaSeconds) noexcept {
    remainingSeconds = (std::max)(0.0f, remainingSeconds - deltaSeconds);
}

[[nodiscard]] bool SegmentIntersectsClosedCell(
    const Float2 start,
    const Float2 end,
    const GridCoordinate cell,
    const float cellSize) noexcept {
    const double minimumX = static_cast<double>(cell.column) * cellSize;
    const double maximumX = minimumX + cellSize;
    const double minimumZ = static_cast<double>(cell.row) * cellSize;
    const double maximumZ = minimumZ + cellSize;
    const double deltaX = static_cast<double>(end.x) - start.x;
    const double deltaZ = static_cast<double>(end.z) - start.z;
    double entryTime = 0.0;
    double exitTime = 1.0;

    const auto clipAxis = [&entryTime, &exitTime](
                              const double origin,
                              const double delta,
                              const double minimum,
                              const double maximum) noexcept {
        if (delta == 0.0) {
            return origin >= minimum && origin <= maximum;
        }

        double nearTime = (minimum - origin) / delta;
        double farTime = (maximum - origin) / delta;
        if (nearTime > farTime) {
            std::swap(nearTime, farTime);
        }
        entryTime = (std::max)(entryTime, nearTime);
        exitTime = (std::min)(exitTime, farTime);
        return entryTime <= exitTime;
    };

    return clipAxis(start.x, deltaX, minimumX, maximumX) &&
           clipAxis(start.z, deltaZ, minimumZ, maximumZ);
}

// Testing every touched closed wall cell is deliberately conservative: a ray
// that runs exactly along an edge or through a wall corner is considered
// blocked, matching a supercover traversal rather than a thin visual ray.
[[nodiscard]] bool HasWallLineOfSight(
    const GridMap& map,
    const Float2 start,
    const Float2 end,
    const float cellSize) {
    if (!map.TryGetCoordinateAtPosition(start, cellSize).has_value() ||
        !map.TryGetCoordinateAtPosition(end, cellSize).has_value()) {
        return false;
    }

    for (std::size_t row = 0; row < map.GetHeight(); ++row) {
        for (std::size_t column = 0; column < map.GetWidth(); ++column) {
            if (!map.IsSolid(
                    static_cast<std::ptrdiff_t>(row),
                    static_cast<std::ptrdiff_t>(column))) {
                continue;
            }
            if (SegmentIntersectsClosedCell(
                    start, end, {row, column}, cellSize)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] std::size_t GridIndex(
    const GridCoordinate coordinate,
    const std::size_t width) noexcept {
    return coordinate.row * width + coordinate.column;
}

[[nodiscard]] std::size_t ManhattanDistance(
    const GridCoordinate left,
    const GridCoordinate right) noexcept {
    const std::size_t rowDistance = left.row > right.row
                                        ? left.row - right.row
                                        : right.row - left.row;
    const std::size_t columnDistance = left.column > right.column
                                           ? left.column - right.column
                                           : right.column - left.column;
    return rowDistance + columnDistance;
}

[[nodiscard]] bool IsNavigationCell(
    const GridMap& map,
    const GridCoordinate coordinate,
    const float radius,
    const float cellSize,
    const std::optional<GridCoordinate> forbiddenCell = std::nullopt) {
    if (coordinate.row >= map.GetHeight() || coordinate.column >= map.GetWidth() ||
        !map.IsWalkable(
            static_cast<std::ptrdiff_t>(coordinate.row),
            static_cast<std::ptrdiff_t>(coordinate.column)) ||
        (forbiddenCell.has_value() && coordinate == *forbiddenCell)) {
        return false;
    }
    return !GridCollision::OverlapsSolid(
        map, map.GetCellCenter(coordinate, cellSize), radius, cellSize);
}

struct OpenNode final {
    GridCoordinate coordinate{};
    std::size_t pathCost = 0;
    std::size_t heuristic = 0;
};

struct OpenNodeCompare final {
    [[nodiscard]] bool operator()(const OpenNode& left, const OpenNode& right) const noexcept {
        const std::size_t leftMaximum =
            (std::numeric_limits<std::size_t>::max)() - left.heuristic;
        const std::size_t rightMaximum =
            (std::numeric_limits<std::size_t>::max)() - right.heuristic;
        const std::size_t leftTotal = left.pathCost > leftMaximum
                                          ? (std::numeric_limits<std::size_t>::max)()
                                          : left.pathCost + left.heuristic;
        const std::size_t rightTotal = right.pathCost > rightMaximum
                                           ? (std::numeric_limits<std::size_t>::max)()
                                           : right.pathCost + right.heuristic;
        if (leftTotal != rightTotal) {
            return leftTotal > rightTotal;
        }
        if (left.heuristic != right.heuristic) {
            return left.heuristic > right.heuristic;
        }
        if (left.coordinate.row != right.coordinate.row) {
            return left.coordinate.row > right.coordinate.row;
        }
        return left.coordinate.column > right.coordinate.column;
    }
};

[[nodiscard]] std::vector<GridCoordinate> FindPath(
    const GridMap& map,
    const GridCoordinate start,
    const GridCoordinate goal,
    const float radius,
    const float cellSize,
    const std::optional<GridCoordinate> forbiddenCell = std::nullopt) {
    const std::optional<GridCoordinate> startForbidden =
        forbiddenCell.has_value() && start == *forbiddenCell
            ? std::nullopt
            : forbiddenCell;
    if (!IsNavigationCell(map, start, radius, cellSize, startForbidden) ||
        !IsNavigationCell(map, goal, radius, cellSize, forbiddenCell)) {
        return {};
    }
    if (start == goal) {
        return {start};
    }

    const std::size_t width = map.GetWidth();
    const std::size_t cellCount = width * map.GetHeight();
    const std::size_t unreachable = (std::numeric_limits<std::size_t>::max)();
    std::vector<std::size_t> costs(cellCount, unreachable);
    std::vector<std::size_t> parents(cellCount, unreachable);
    std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeCompare> open;

    costs[GridIndex(start, width)] = 0;
    open.push({start, 0, ManhattanDistance(start, goal)});

    constexpr std::array<std::array<std::ptrdiff_t, 2>, 4> kNeighbors{{
        {{-1, 0}},
        {{0, -1}},
        {{0, 1}},
        {{1, 0}},
    }};

    while (!open.empty()) {
        const OpenNode current = open.top();
        open.pop();
        const std::size_t currentIndex = GridIndex(current.coordinate, width);
        if (current.pathCost != costs[currentIndex]) {
            continue;
        }
        if (current.coordinate == goal) {
            std::vector<GridCoordinate> result;
            std::size_t index = currentIndex;
            const std::size_t startIndex = GridIndex(start, width);
            while (true) {
                result.push_back({index / width, index % width});
                if (index == startIndex) {
                    break;
                }
                index = parents[index];
                if (index == unreachable) {
                    return {};
                }
            }
            std::reverse(result.begin(), result.end());
            return result;
        }

        for (const auto& offset : kNeighbors) {
            const std::ptrdiff_t nextRow =
                static_cast<std::ptrdiff_t>(current.coordinate.row) + offset[0];
            const std::ptrdiff_t nextColumn =
                static_cast<std::ptrdiff_t>(current.coordinate.column) + offset[1];
            if (nextRow < 0 || nextColumn < 0) {
                continue;
            }
            const GridCoordinate next{
                static_cast<std::size_t>(nextRow),
                static_cast<std::size_t>(nextColumn),
            };
            if (!IsNavigationCell(map, next, radius, cellSize, forbiddenCell)) {
                continue;
            }

            const std::size_t nextIndex = GridIndex(next, width);
            const std::size_t nextCost = current.pathCost + 1;
            if (nextCost >= costs[nextIndex]) {
                continue;
            }
            costs[nextIndex] = nextCost;
            parents[nextIndex] = currentIndex;
            open.push({next, nextCost, ManhattanDistance(next, goal)});
        }
    }

    return {};
}

[[nodiscard]] bool IsPathValid(
    const GridMap& map,
    const std::vector<GridCoordinate>& path,
    const std::size_t nextWaypointIndex,
    const float radius,
    const float cellSize,
    const std::optional<GridCoordinate> forbiddenCell = std::nullopt) {
    if (path.empty() || nextWaypointIndex > path.size()) {
        return false;
    }
    for (std::size_t index = nextWaypointIndex; index < path.size(); ++index) {
        if (!IsNavigationCell(
                map, path[index], radius, cellSize, forbiddenCell)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Float2 MoveToward(
    const Float2 position,
    const Float2 target,
    const float maximumDistance,
    const float radius,
    const float height,
    std::span<const Engine::Collision::Aabb> walls,
    std::span<const Engine::Collision::VerticalCapsule> actors) {
    const float targetDistance = Distance(position, target);
    if (targetDistance <= kPositionEpsilon || maximumDistance <= 0.0f) {
        return position;
    }
    const float distanceToMove = (std::min)(maximumDistance, targetDistance);
    const float scale = distanceToMove / targetDistance;
    const Float3 displacement{
        (target.x - position.x) * scale,
        0.0f,
        (target.z - position.z) * scale,
    };
    const auto moved = MoveCharacterBody({{position.x, 0, position.z}, height, radius},
        displacement, walls, actors, true);
    // This product's enemy locomotion stays on its authored flat floor.
    return {moved.x, moved.z};
}

[[nodiscard]] bool ValidatePositive(
    const float value,
    const char* const label,
    std::string& error) {
    if (std::isfinite(value) && value > 0.0f) {
        return true;
    }
    error = "EnemySettings ";
    error += label;
    error += " must be finite and greater than zero.";
    return false;
}

[[nodiscard]] bool ValidateNonNegative(
    const float value,
    const char* const label,
    std::string& error) {
    if (std::isfinite(value) && value >= 0.0f) {
        return true;
    }
    error = "EnemySettings ";
    error += label;
    error += " must be finite and non-negative.";
    return false;
}

} // namespace

bool ValidateEnemySettings(
    const EnemySettings& settings,
    std::string& error) {
    error.clear();
    if (!ValidatePositive(
            settings.repathIntervalSeconds, "repath interval", error) ||
        !ValidatePositive(settings.meleeSpeed, "melee speed", error) ||
        !ValidateNonNegative(
            settings.meleeAttackSurfaceDistance,
            "melee attack surface distance",
            error) ||
        !ValidatePositive(settings.rangedSpeed, "ranged speed", error) ||
        !ValidateNonNegative(
            settings.rangedTooCloseSurfaceDistance,
            "ranged too-close surface distance",
            error) ||
        !ValidatePositive(
            settings.rangedIdealSurfaceDistance,
            "ranged ideal surface distance",
            error) ||
        !ValidatePositive(
            settings.rangedMaximumAttackSurfaceDistance,
            "ranged maximum attack surface distance",
            error)) {
        return false;
    }

    if (settings.rangedTooCloseSurfaceDistance >=
        settings.rangedIdealSurfaceDistance) {
        error = "EnemySettings ranged too-close distance must be less than the ideal distance.";
        return false;
    }
    if (settings.rangedIdealSurfaceDistance >
        settings.rangedMaximumAttackSurfaceDistance) {
        error = "EnemySettings ranged ideal distance must not exceed the maximum attack distance.";
        return false;
    }
    return true;
}

bool EnemySystem::Initialize(
    const GridMap& map,
    const Float2 playerPosition,
    const float playerCollisionRadius,
    const float cellSize,
    EnemySettings settings,
    std::string& error,
    const float wallHeight,
    const float playerHitboxHeight) {
    Reset();
    error.clear();

    if (!ValidateEnemySettings(settings, error)) {
        return false;
    }
    if (!IsFinite(playerPosition)) {
        error = "EnemySystem player position must be finite.";
        return false;
    }
    if (!std::isfinite(playerCollisionRadius) || playerCollisionRadius <= 0.0f) {
        error = "EnemySystem player collision radius must be finite and greater than zero.";
        return false;
    }
    if (!std::isfinite(cellSize) || cellSize <= 0.0f) {
        error = "EnemySystem cell size must be finite and greater than zero.";
        return false;
    }
    if (!std::isfinite(wallHeight) || wallHeight <= 0.0f ||
        !std::isfinite(playerHitboxHeight) || playerHitboxHeight < playerCollisionRadius * 2.0f) {
        error = "EnemySystem world wall height and player capsule height must be valid.";
        return false;
    }
    if (map.GetWidth() == 0 || map.GetHeight() == 0 ||
        map.GetWidth() >
            (std::numeric_limits<std::size_t>::max)() / map.GetHeight()) {
        error = "EnemySystem requires a non-empty map with a supported cell count.";
        return false;
    }

    try {
        if (GridCollision::OverlapsSolid(
                map, playerPosition, playerCollisionRadius, cellSize)) {
            error = "EnemySystem player collider overlaps a solid map cell.";
            return false;
        }
    } catch (const std::exception& exception) {
        error = "Failed to initialize enemies: ";
        error += exception.what();
        return false;
    }

    settings_ = std::move(settings);
    cellSize_ = cellSize;
    wallHeight_ = wallHeight;
    playerHitboxHeight_ = playerHitboxHeight;
    mapWidth_ = map.GetWidth();
    mapHeight_ = map.GetHeight();
    nextEnemyId_ = 1;
    initialized_ = true;
    return true;
}

bool EnemySystem::ValidateDefinition(
    const EnemyDefinition& definition,
    std::string& error) const {
    error.clear();
    if (definition.id.empty() ||
        (definition.kind != EnemyKind::Melee && definition.kind != EnemyKind::Ranged)) {
        error = "Enemy definition requires a valid ID and supported kind.";
        return false;
    }
    if (!std::isfinite(definition.damage) || definition.damage <= 0.0f ||
        !std::isfinite(definition.attackIntervalSeconds) || definition.attackIntervalSeconds <= 0.0f ||
        !std::isfinite(definition.maxHealth) || definition.maxHealth <= 0.0f ||
        !std::isfinite(definition.defense) || definition.defense < 0.0f ||
        !std::isfinite(definition.hitboxRadius) || definition.hitboxRadius <= 0.0f ||
        !std::isfinite(definition.hitboxHeight) || definition.hitboxHeight < definition.hitboxRadius * 2.0f ||
        !definition.presentationAssetId.IsValid() || !definition.rig || !definition.rig->model) {
        error = "Enemy definition requires valid combat values, body capsule, and resolved 3D rig.";
        return false;
    }
    const auto& rig = *definition.rig;
    if (!std::isfinite(rig.scale) || rig.scale <= 0 ||
        !std::isfinite(rig.transitionSeconds) || rig.transitionSeconds < 0 ||
        !std::isfinite(rig.anchor.x) || !std::isfinite(rig.anchor.y) || !std::isfinite(rig.anchor.z)) {
        error = "Enemy rig calibration and transition duration must be finite and valid.";
        return false;
    }
    for (const auto clip : rig.clips) {
        if (clip >= rig.model->clips.size() ||
            !std::isfinite(rig.model->clips[clip].durationSeconds) ||
            rig.model->clips[clip].durationSeconds <= 0) {
            error = "Enemy rig requires valid idle, move, attack and death clips.";
            return false;
        }
    }
    const double duration = rig.model->clips[rig.clips[2]].durationSeconds;
    if (definition.attackIntervalSeconds + 0.000001 < duration ||
        !std::isfinite(rig.attackBeginSeconds) || !std::isfinite(rig.attackEndSeconds) ||
        rig.attackBeginSeconds < 0 || rig.attackEndSeconds < rig.attackBeginSeconds ||
        rig.attackEndSeconds > duration || !std::isfinite(rig.releaseSeconds) ||
        rig.releaseSeconds < 0 || rig.releaseSeconds > duration) {
        error = "Enemy attack interval, active window and release event must fit the attack clip.";
        return false;
    }
    const auto validPoint = [&rig](const EnemyBonePoint& point) {
        return point.node < rig.model->nodes.size() && std::isfinite(point.offset.x) &&
            std::isfinite(point.offset.y) && std::isfinite(point.offset.z);
    };
    if (rig.hurtRegions.empty() || !validPoint(rig.attackPoint) ||
        !std::isfinite(rig.attackRadius) || rig.attackRadius <= 0) {
        error = "Enemy rig requires hurt regions and a valid attack attachment.";
        return false;
    }
    for (const auto& region : rig.hurtRegions) {
        if (region.id.empty() || !validPoint(region.start) || !validPoint(region.end) ||
            !std::isfinite(region.radius) || region.radius <= 0) {
            error = "Enemy hurt regions require a name, valid bone endpoints and positive radius.";
            return false;
        }
    }
    return true;
}
void EnemySystem::Reset() noexcept {
    settings_ = {};
    cellSize_ = 1.0f;
    wallHeight_ = 2.5f;
    playerHitboxHeight_ = 1.8f;
    mapWidth_ = 0;
    mapHeight_ = 0;
    nextEnemyId_ = 1;
    initialized_ = false;
    enemies_.clear();
    snapshots_.clear();
    attackEvents_.clear();
}

EnemySpawnResult EnemySystem::Spawn(
    const GridMap& map,
    const Float2 playerPosition,
    const float playerCollisionRadius,
    const Float2 spawnPosition,
    const EnemyDefinition& definition,
    std::string& error) {
    error.clear();
    if (!initialized_) {
        error = "EnemySystem must be initialized before spawning enemies.";
        return {};
    }
    if (map.GetWidth() != mapWidth_ || map.GetHeight() != mapHeight_) {
        error = "Enemy spawn map dimensions differ from the initialized map.";
        return {};
    }
    if (!IsFinite(playerPosition) ||
        !std::isfinite(playerCollisionRadius) || playerCollisionRadius <= 0.0f ||
        !IsFinite(spawnPosition)) {
        error = "Enemy spawn positions and player radius must be finite and valid.";
        return {};
    }
    if (!ValidateDefinition(definition, error)) {
        return {};
    }
    if (nextEnemyId_ == (std::numeric_limits<EnemyId>::max)()) {
        error = "EnemySystem exhausted its stable runtime IDs.";
        return {};
    }

    try {
        const auto walls = BuildWorldCollisionBoxes(map, {cellSize_, wallHeight_});
        std::vector<Engine::Collision::VerticalCapsule> occupied{
            {{playerPosition.x, 0, playerPosition.z}, playerHitboxHeight_, playerCollisionRadius}};
        for (const auto& other : enemies_) {
            if (other.state != EnemyState::Dead || other.stateElapsedSeconds < DeadVisibilitySeconds(other.definition)) {
                occupied.push_back({{other.position.x, 0, other.position.z},
                    other.definition.hitboxHeight, other.definition.hitboxRadius});
            }
        }
        if (!map.TryGetCoordinateAtPosition(spawnPosition, cellSize_) ||
            !CanPlaceCharacterBody({{spawnPosition.x, 0, spawnPosition.z},
                definition.hitboxHeight, definition.hitboxRadius}, walls, occupied)) {
            return {EnemySpawnStatus::Blocked, 0};
        }

        RuntimeEnemy enemy{};
        enemy.id = nextEnemyId_++;
        enemy.kind = definition.kind;
        enemy.position = spawnPosition;
        enemy.yawRadians = std::atan2(playerPosition.x - spawnPosition.x, playerPosition.z - spawnPosition.z);
        enemy.definition = definition;
        enemy.animation = Engine::Model::AnimationInstance(definition.rig->model);
        const auto animation = enemy.animation.Play(definition.rig->clips[0], Engine::Model::PlaybackMode::Loop);
        if (!animation) throw std::runtime_error("Enemy idle animation: " + animation.error());
        enemy.health = definition.maxHealth;
        enemy.repathElapsedSeconds = settings_.repathIntervalSeconds;
        const EnemyId spawnedId = enemy.id;
        enemies_.push_back(std::move(enemy));
        RefreshSnapshots();
        return {EnemySpawnStatus::Spawned, spawnedId};
    } catch (const std::exception& exception) {
        error = "Failed to spawn enemy: ";
        error += exception.what();
    } catch (...) {
        error = "Failed to spawn enemy because of an unknown error.";
    }
    return {};
}

bool EnemySystem::Retire(const EnemyId id) noexcept {
    for (std::size_t index = 0; index < enemies_.size(); ++index) {
        if (enemies_[index].id != id) {
            continue;
        }
        enemies_.erase(enemies_.begin() + static_cast<std::ptrdiff_t>(index));
        snapshots_.erase(snapshots_.begin() + static_cast<std::ptrdiff_t>(index));
        std::erase_if(
            attackEvents_,
            [id](const EnemyAttackEvent& event) { return event.enemyId == id; });
        return true;
    }
    return false;
}

bool EnemySystem::RetireDead(const EnemyId id) noexcept {
    for (const RuntimeEnemy& enemy : enemies_) {
        if (enemy.id == id) {
            return enemy.state == EnemyState::Dead &&
                   enemy.stateElapsedSeconds >=
                       DeadVisibilitySeconds(enemy.definition) &&
                   Retire(id);
        }
    }
    return false;
}

std::size_t EnemySystem::RetireExpiredDead() noexcept {
    std::size_t retiredCount = 0;
    for (std::size_t index = enemies_.size(); index > 0; --index) {
        const RuntimeEnemy& enemy = enemies_[index - 1];
        if (enemy.state == EnemyState::Dead &&
            enemy.stateElapsedSeconds >= DeadVisibilitySeconds(enemy.definition)) {
            const EnemyId id = enemy.id;
            static_cast<void>(Retire(id));
            ++retiredCount;
        }
    }
    return retiredCount;
}

void EnemySystem::Update(
    const GridMap& map,
    const EnemyTarget& player,
    const float deltaSeconds) {
    attackEvents_.clear();
    if (!initialized_ || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        return;
    }
    const Float2 playerPosition = player.position;
    const float playerCollisionRadius = player.collisionRadius;
    if (!IsFinite(playerPosition)) {
        throw std::invalid_argument("enemy update player position must be finite");
    }
    if (!std::isfinite(playerCollisionRadius) || playerCollisionRadius <= 0.0f) {
        throw std::invalid_argument(
            "enemy update player collision radius must be finite and greater than zero");
    }
    if (!std::isfinite(player.feetY) || !std::isfinite(player.hitboxHeight) || player.hitboxHeight < playerCollisionRadius * 2.0f) {
        throw std::invalid_argument(
            "enemy update player hitbox height must be finite and greater than zero");
    }
    if (map.GetWidth() != mapWidth_ || map.GetHeight() != mapHeight_) {
        throw std::invalid_argument(
            "enemy update map dimensions differ from the initialized map");
    }

    const std::optional<GridCoordinate> playerCell =
        map.TryGetCoordinateAtPosition(playerPosition, cellSize_);
    if (!playerCell.has_value()) {
        throw std::invalid_argument("enemy update player position is outside the map");
    }

    const auto walls = BuildWorldCollisionBoxes(map, {cellSize_, wallHeight_});
    const Engine::Collision::VerticalCapsule playerBody{
        {playerPosition.x, player.feetY, playerPosition.z}, player.hitboxHeight, playerCollisionRadius};
    const auto setState = [this](RuntimeEnemy& enemy, const EnemyState state) {
        SetState(enemy, state);
    };
    const auto clearNavigation = [](RuntimeEnemy& enemy) noexcept {
        enemy.navigationPurpose = NavigationPurpose::None;
        enemy.path.clear();
        enemy.nextWaypointIndex = 0;
        enemy.lastPlayerCell.reset();
        enemy.repathElapsedSeconds = 0.0f;
        enemy.stuckElapsedSeconds = 0.0f;
    };
    const auto assignPath = [playerCell](
                                RuntimeEnemy& enemy,
                                const NavigationPurpose purpose,
                                std::vector<GridCoordinate> path) {
        enemy.navigationPurpose = purpose;
        enemy.path = std::move(path);
        enemy.nextWaypointIndex = enemy.path.size() > 1 ? 1 : 0;
        enemy.lastPlayerCell = playerCell;
        enemy.repathElapsedSeconds = 0.0f;
        enemy.stuckElapsedSeconds = 0.0f;
    };
    const auto enterAttack = [&setState, &clearNavigation, playerPosition](RuntimeEnemy& enemy) {
        setState(enemy, EnemyState::Attacking);
        enemy.yawRadians = std::atan2(playerPosition.x - enemy.position.x, playerPosition.z - enemy.position.z);
        enemy.attackEventEmitted = false;
        enemy.attackCooldownSeconds = enemy.definition.attackIntervalSeconds;
        clearNavigation(enemy);
    };
    for (RuntimeEnemy& enemy : enemies_) {
        SubtractElapsed(enemy.hitFlashRemainingSeconds, deltaSeconds);
        if (enemy.state == EnemyState::Dead) continue;
        SubtractElapsed(enemy.attackCooldownSeconds, deltaSeconds);
        AddElapsed(enemy.repathElapsedSeconds, deltaSeconds);
        if (enemy.state == EnemyState::Attacking) {
            if (!enemy.animation.IsFinished()) continue;
            setState(enemy, EnemyState::Idle);
        }
        // Stable insertion/ID order sees already-resolved earlier actors and
        // previous positions of later actors. No enemy is allowed to pass through another.
        std::vector<Engine::Collision::VerticalCapsule> actors{playerBody};
        actors.reserve(enemies_.size());
        for (const auto& other : enemies_) {
            if (other.id != enemy.id && other.state != EnemyState::Dead)
                actors.push_back({{other.position.x, 0, other.position.z},
                    other.definition.hitboxHeight, other.definition.hitboxRadius});
        }
        const float surfaceDistance = SurfaceDistance(
            enemy.position,
            enemy.definition.hitboxRadius,
            playerPosition,
            playerCollisionRadius);
        const bool hasLineOfSight = HasWallLineOfSight(
            map, enemy.position, playerPosition, cellSize_);
        const std::optional<GridCoordinate> enemyCell =
            map.TryGetCoordinateAtPosition(enemy.position, cellSize_);

        bool movementRequested = false;
        const Float2 previousPosition = enemy.position;

        if (enemy.kind == EnemyKind::Melee) {
            if (surfaceDistance <= settings_.meleeAttackSurfaceDistance &&
                player.feetY < enemy.definition.hitboxHeight &&
                player.feetY + player.hitboxHeight > 0.0f && hasLineOfSight) {
                clearNavigation(enemy);
                if (enemy.attackCooldownSeconds <= 0.0f) {
                    enterAttack(enemy);
                } else {
                    setState(enemy, EnemyState::Idle);
                }
                continue;
            }

            if (hasLineOfSight) {
                clearNavigation(enemy);
                movementRequested = true;
                enemy.position = MoveToward(
                    enemy.position,
                    playerPosition,
                    settings_.meleeSpeed * deltaSeconds,
                    enemy.definition.hitboxRadius,
                    enemy.definition.hitboxHeight, walls, actors);
            } else if (enemyCell.has_value()) {
                const bool needsPath =
                    enemy.navigationPurpose != NavigationPurpose::Chase ||
                    enemy.path.empty() ||
                    enemy.nextWaypointIndex >= enemy.path.size() ||
                    enemy.repathElapsedSeconds >= settings_.repathIntervalSeconds ||
                    enemy.stuckElapsedSeconds >= settings_.repathIntervalSeconds ||
                    !enemy.lastPlayerCell.has_value() ||
                    *enemy.lastPlayerCell != *playerCell ||
                    !IsPathValid(
                        map,
                        enemy.path,
                        enemy.nextWaypointIndex,
                        enemy.definition.hitboxRadius,
                        cellSize_);
                if (needsPath) {
                    assignPath(
                        enemy,
                        NavigationPurpose::Chase,
                        FindPath(
                            map,
                            *enemyCell,
                            *playerCell,
                            enemy.definition.hitboxRadius,
                            cellSize_));
                }
            } else {
                clearNavigation(enemy);
            }
        } else if (surfaceDistance <
                   settings_.rangedTooCloseSurfaceDistance) {
            if (enemyCell.has_value()) {
                const bool needsPath =
                    enemy.navigationPurpose != NavigationPurpose::Retreat ||
                    enemy.path.empty() ||
                    enemy.nextWaypointIndex >= enemy.path.size() ||
                    enemy.repathElapsedSeconds >= settings_.repathIntervalSeconds ||
                    enemy.stuckElapsedSeconds >= settings_.repathIntervalSeconds ||
                    !enemy.lastPlayerCell.has_value() ||
                    *enemy.lastPlayerCell != *playerCell ||
                    !IsPathValid(
                        map,
                        enemy.path,
                        enemy.nextWaypointIndex,
                        enemy.definition.hitboxRadius,
                        cellSize_,
                        playerCell);
                if (needsPath) {
                    std::vector<GridCoordinate> bestPath;
                    float bestIdealDifference =
                        (std::numeric_limits<float>::max)();
                    std::size_t bestPathCost =
                        (std::numeric_limits<std::size_t>::max)();
                    GridCoordinate bestCell{
                        (std::numeric_limits<std::size_t>::max)(),
                        (std::numeric_limits<std::size_t>::max)(),
                    };
                    const float currentIdealDifference = std::fabs(
                        surfaceDistance - settings_.rangedIdealSurfaceDistance);

                    for (std::size_t row = 0; row < map.GetHeight(); ++row) {
                        for (std::size_t column = 0;
                             column < map.GetWidth();
                             ++column) {
                            const GridCoordinate candidate{row, column};
                            if (!IsNavigationCell(
                                    map,
                                    candidate,
                                    enemy.definition.hitboxRadius,
                                    cellSize_,
                                    playerCell)) {
                                continue;
                            }
                            const Float2 candidatePosition =
                                map.GetCellCenter(candidate, cellSize_);
                            const float candidateSurfaceDistance = SurfaceDistance(
                                candidatePosition,
                                enemy.definition.hitboxRadius,
                                playerPosition,
                                playerCollisionRadius);
                            const float idealDifference = std::fabs(
                                candidateSurfaceDistance -
                                settings_.rangedIdealSurfaceDistance);
                            if (candidateSurfaceDistance <=
                                    surfaceDistance + kPositionEpsilon ||
                                idealDifference >=
                                    currentIdealDifference - kPositionEpsilon) {
                                continue;
                            }

                            std::vector<GridCoordinate> candidatePath = FindPath(
                                map,
                                *enemyCell,
                                candidate,
                                enemy.definition.hitboxRadius,
                                cellSize_,
                                playerCell);
                            if (candidatePath.empty()) {
                                continue;
                            }
                            const std::size_t pathCost = candidatePath.size() - 1;
                            const bool better =
                                idealDifference <
                                    bestIdealDifference - kPositionEpsilon ||
                                (std::fabs(
                                     idealDifference - bestIdealDifference) <=
                                     kPositionEpsilon &&
                                 (pathCost < bestPathCost ||
                                  (pathCost == bestPathCost &&
                                   (candidate.row < bestCell.row ||
                                    (candidate.row == bestCell.row &&
                                     candidate.column < bestCell.column)))));
                            if (better) {
                                bestIdealDifference = idealDifference;
                                bestPathCost = pathCost;
                                bestCell = candidate;
                                bestPath = std::move(candidatePath);
                            }
                        }
                    }
                    assignPath(
                        enemy,
                        NavigationPurpose::Retreat,
                        std::move(bestPath));
                }
            } else {
                clearNavigation(enemy);
            }
        } else if (surfaceDistance <=
                       settings_.rangedMaximumAttackSurfaceDistance &&
                   hasLineOfSight) {
            clearNavigation(enemy);
            if (enemy.attackCooldownSeconds <= 0.0f) {
                enterAttack(enemy);
            } else {
                setState(enemy, EnemyState::Idle);
            }
            continue;
        } else if (enemyCell.has_value()) {
            const bool needsPath =
                enemy.navigationPurpose != NavigationPurpose::FiringPosition ||
                enemy.path.empty() ||
                enemy.nextWaypointIndex >= enemy.path.size() ||
                enemy.repathElapsedSeconds >= settings_.repathIntervalSeconds ||
                enemy.stuckElapsedSeconds >= settings_.repathIntervalSeconds ||
                !enemy.lastPlayerCell.has_value() ||
                *enemy.lastPlayerCell != *playerCell ||
                !IsPathValid(
                    map,
                    enemy.path,
                    enemy.nextWaypointIndex,
                    enemy.definition.hitboxRadius,
                    cellSize_,
                    playerCell);
            if (needsPath) {
                std::vector<GridCoordinate> bestPath;
                std::size_t bestPathCost =
                    (std::numeric_limits<std::size_t>::max)();
                float bestIdealDifference =
                    (std::numeric_limits<float>::max)();
                GridCoordinate bestCell{
                    (std::numeric_limits<std::size_t>::max)(),
                    (std::numeric_limits<std::size_t>::max)(),
                };

                for (std::size_t row = 0; row < map.GetHeight(); ++row) {
                    for (std::size_t column = 0;
                         column < map.GetWidth();
                         ++column) {
                        const GridCoordinate candidate{row, column};
                        if (!IsNavigationCell(
                                map,
                                candidate,
                                enemy.definition.hitboxRadius,
                                cellSize_,
                                playerCell)) {
                            continue;
                        }
                        const Float2 candidatePosition =
                            map.GetCellCenter(candidate, cellSize_);
                        const float candidateSurfaceDistance = SurfaceDistance(
                            candidatePosition,
                            enemy.definition.hitboxRadius,
                            playerPosition,
                            playerCollisionRadius);
                        if (candidateSurfaceDistance <
                                settings_.rangedTooCloseSurfaceDistance ||
                            candidateSurfaceDistance >
                                settings_.rangedMaximumAttackSurfaceDistance ||
                            !HasWallLineOfSight(
                                map,
                                candidatePosition,
                                playerPosition,
                                cellSize_)) {
                            continue;
                        }

                        std::vector<GridCoordinate> candidatePath = FindPath(
                            map,
                            *enemyCell,
                            candidate,
                            enemy.definition.hitboxRadius,
                            cellSize_,
                            playerCell);
                        if (candidatePath.empty()) {
                            continue;
                        }
                        const std::size_t pathCost = candidatePath.size() - 1;
                        const float idealDifference = std::fabs(
                            candidateSurfaceDistance -
                            settings_.rangedIdealSurfaceDistance);
                        const bool better =
                            pathCost < bestPathCost ||
                            (pathCost == bestPathCost &&
                             (idealDifference <
                                  bestIdealDifference - kPositionEpsilon ||
                              (std::fabs(
                                   idealDifference - bestIdealDifference) <=
                                   kPositionEpsilon &&
                               (candidate.row < bestCell.row ||
                                (candidate.row == bestCell.row &&
                                 candidate.column < bestCell.column)))));
                        if (better) {
                            bestPathCost = pathCost;
                            bestIdealDifference = idealDifference;
                            bestCell = candidate;
                            bestPath = std::move(candidatePath);
                        }
                    }
                }
                assignPath(
                    enemy,
                    NavigationPurpose::FiringPosition,
                    std::move(bestPath));
            }
        } else {
            clearNavigation(enemy);
        }

        if (!movementRequested && !enemy.path.empty() &&
            enemy.nextWaypointIndex < enemy.path.size()) {
            float movementBudget =
                (enemy.kind == EnemyKind::Melee
                     ? settings_.meleeSpeed
                     : settings_.rangedSpeed) *
                deltaSeconds;
            std::size_t remainingIterations =
                enemy.path.size() - enemy.nextWaypointIndex + 1;
            while (movementBudget > kPositionEpsilon &&
                   enemy.nextWaypointIndex < enemy.path.size() &&
                   remainingIterations > 0) {
                --remainingIterations;
                const Float2 waypoint = map.GetCellCenter(
                    enemy.path[enemy.nextWaypointIndex], cellSize_);
                const float waypointDistance = Distance(enemy.position, waypoint);
                if (waypointDistance <= kWaypointTolerance) {
                    ++enemy.nextWaypointIndex;
                    continue;
                }

                movementRequested = true;
                const Float2 beforeStep = enemy.position;
                enemy.position = MoveToward(
                    enemy.position,
                    waypoint,
                    movementBudget,
                    enemy.definition.hitboxRadius,
                    enemy.definition.hitboxHeight, walls, actors);
                const float movedDistance = Distance(beforeStep, enemy.position);
                if (movedDistance <= kPositionEpsilon) {
                    break;
                }
                movementBudget =
                    (std::max)(0.0f, movementBudget - movedDistance);
                if (Distance(enemy.position, waypoint) <= kWaypointTolerance) {
                    ++enemy.nextWaypointIndex;
                }
                if (movedDistance + kPositionEpsilon <
                    (std::min)(movementBudget + movedDistance, waypointDistance)) {
                    break;
                }
            }
        }

        if (Distance(previousPosition, enemy.position) > kPositionEpsilon) {
            enemy.yawRadians = std::atan2(enemy.position.x - previousPosition.x, enemy.position.z - previousPosition.z);
        }
        const float movedDistance = Distance(previousPosition, enemy.position);
        if (movementRequested && movedDistance <= kPositionEpsilon) {
            AddElapsed(enemy.stuckElapsedSeconds, deltaSeconds);
        } else {
            enemy.stuckElapsedSeconds = 0.0f;
        }
        setState(
            enemy,
            movedDistance > kPositionEpsilon
                ? EnemyState::Moving
                : EnemyState::Idle);
    }

    for (auto& enemy : enemies_) AdvanceAnimation(enemy, player, walls, deltaSeconds);
    RefreshSnapshots();
}

void EnemySystem::SetState(RuntimeEnemy& enemy, const EnemyState state) {
    if (enemy.state == state) return;
    const auto& rig = *enemy.definition.rig;
    const auto mode = state == EnemyState::Idle || state == EnemyState::Moving
        ? Engine::Model::PlaybackMode::Loop : Engine::Model::PlaybackMode::Clamp;
    const auto result = enemy.animation.Play(rig.clips[static_cast<std::size_t>(state)], mode, rig.transitionSeconds);
    if (!result) throw std::runtime_error("Enemy animation transition: " + result.error());
    enemy.state = state;
    enemy.stateElapsedSeconds = 0;
    enemy.attackShape.reset();
}

void EnemySystem::AdvanceAnimation(RuntimeEnemy& enemy, const EnemyTarget& player,
    const std::span<const Engine::Collision::Aabb> walls, const float deltaSeconds) {
    // Preserve the complete blend state, so an event inside a long simulation
    // tick samples precisely the same pose as advancing normally to that time.
    auto eventAnimation = enemy.animation;
    const auto advance = enemy.animation.Advance(deltaSeconds);
    if (!advance) throw std::runtime_error("Enemy animation advance: " + advance.error());
    AddElapsed(enemy.stateElapsedSeconds, deltaSeconds);
    enemy.attackShape.reset();
    if (enemy.state != EnemyState::Attacking) return;

    const auto& rig = *enemy.definition.rig;
    const double previous = advance.value().previousSeconds;
    const double current = advance.value().currentSeconds;
    const auto pointAt = [&](const double seconds) {
        const auto result = eventAnimation.Advance((std::max)(0.0, seconds - eventAnimation.TimeSeconds()));
        if (!result) throw std::runtime_error("Enemy attack pose: " + result.error());
        return EnemyBoneWorldPoint(rig, eventAnimation.CurrentPose(), rig.attackPoint, enemy.position, enemy.yawRadians);
    };
    const auto emit = [&](const Float3 origin) {
        enemy.attackEventEmitted = true;
        attackEvents_.push_back({enemy.id, enemy.definition.id, enemy.kind, origin,
            {player.position.x, player.feetY + player.hitboxHeight * 0.5f, player.position.z}, enemy.definition.damage});
    };
    if (enemy.kind == EnemyKind::Ranged) {
        if (!enemy.attackEventEmitted && previous <= rig.releaseSeconds && current >= rig.releaseSeconds) {
            const auto origin = pointAt(rig.releaseSeconds);
            // A hand on the far side of a wall must not spawn a projectile through it.
            const Engine::Collision::Float3 from{enemy.position.x, origin.y, enemy.position.z};
            const Engine::Collision::Float3 direction{origin.x-from.x, 0, origin.z-from.z};
            const float length = std::hypot(direction.x, direction.z);
            bool blocked = false;
            if (length > kPositionEpsilon)
                for (const auto& wall : walls)
                    if (Engine::Collision::RaycastAabb(from, direction, length, wall)) { blocked = true; break; }
            enemy.attackEventEmitted = true;
            if (!blocked) emit(origin);
        }
        return;
    }

    const double begin = (std::max)(previous, rig.attackBeginSeconds);
    const double end = (std::min)(current, rig.attackEndSeconds);
    if (end < begin || current < rig.attackBeginSeconds || previous > rig.attackEndSeconds) return;
    const Engine::Collision::VerticalCapsule playerBody{{player.position.x, player.feetY, player.position.z},
        player.hitboxHeight, player.collisionRadius};
    const float radius = rig.attackRadius * rig.scale;
    Float3 from = pointAt(begin);
    const std::size_t steps = (std::max)(std::size_t{1}, static_cast<std::size_t>(std::ceil((end - begin) * 120.0)));
    for (std::size_t step = 1; step <= steps; ++step) {
        const auto to = pointAt(begin + (end - begin) * static_cast<double>(step) / static_cast<double>(steps));
        const Engine::Collision::Float3 start{from.x,from.y,from.z}, finish{to.x,to.y,to.z};
        const Engine::Collision::Capsule swept{start,finish,radius};
        // The debug shape is the same latest active hand segment used here.
        if (current <= rig.attackEndSeconds) enemy.attackShape = swept;
        if (!enemy.attackEventEmitted) {
            const auto hit = Engine::Collision::SweepSphereAgainstCapsule(start, finish, radius, playerBody);
            if (hit) {
                const Engine::Collision::Float3 displacement{finish.x-start.x,finish.y-start.y,finish.z-start.z};
                const Engine::Collision::VerticalCapsule sphere{{start.x,start.y-radius,start.z},2*radius,radius};
                const Float3 impact{from.x+(to.x-from.x)* *hit,from.y+(to.y-from.y)* *hit,from.z+(to.z-from.z)* *hit};
                const Engine::Collision::Float3 shoulder{enemy.position.x,impact.y,enemy.position.z};
                const Engine::Collision::Float3 reach{impact.x-shoulder.x,0,impact.z-shoulder.z};
                const float reachLength = std::hypot(reach.x, reach.z);
                bool blocked = false;
                for (const auto& wall : walls) {
                    const auto contact = Engine::Collision::SweepVerticalCapsuleAgainstAabb(sphere, displacement, wall);
                    if ((contact && contact->fraction <= *hit) ||
                        (reachLength > kPositionEpsilon && Engine::Collision::RaycastAabb(shoulder, reach, reachLength, wall))) {
                        blocked = true;
                        break;
                    }
                }
                if (!blocked) emit(impact);
            }
        }
        from = to;
    }
}

std::vector<Engine::Collision::VerticalCapsule> EnemySystem::CollectAliveBodies() const {
    std::vector<Engine::Collision::VerticalCapsule> result;
    result.reserve(enemies_.size());
    for (const auto& enemy : enemies_) {
        if (enemy.state != EnemyState::Dead)
            result.push_back({{enemy.position.x,0,enemy.position.z},enemy.definition.hitboxHeight,enemy.definition.hitboxRadius});
    }
    return result;
}

std::vector<CircleObstacle> EnemySystem::CollectAliveColliders() const {
    std::vector<CircleObstacle> colliders;
    colliders.reserve(enemies_.size());
    for (const RuntimeEnemy& enemy : enemies_) {
        if (enemy.state != EnemyState::Dead) {
            colliders.push_back({
                enemy.position,
                enemy.definition.hitboxRadius,
            });
        }
    }
    return colliders;
}

std::vector<CircleObstacle> EnemySystem::CollectOccupiedColliders() const {
    std::vector<CircleObstacle> colliders;
    colliders.reserve(enemies_.size());
    for (const RuntimeEnemy& enemy : enemies_) {
        if (enemy.state != EnemyState::Dead ||
            enemy.stateElapsedSeconds < DeadVisibilitySeconds(enemy.definition)) {
            colliders.push_back({
                enemy.position,
                enemy.definition.hitboxRadius,
            });
        }
    }
    return colliders;
}

std::size_t EnemySystem::GetAliveCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        enemies_.begin(),
        enemies_.end(),
        [](const RuntimeEnemy& enemy) {
            return enemy.state != EnemyState::Dead;
        }));
}

EnemyDamageResult EnemySystem::ApplyDamage(
    const EnemyId id,
    const float rawDamage) {
    EnemyDamageResult result{};
    result.rawDamage = rawDamage;
    if (!std::isfinite(rawDamage) || rawDamage <= 0.0f) {
        return result;
    }

    for (std::size_t index = 0; index < enemies_.size(); ++index) {
        RuntimeEnemy& enemy = enemies_[index];
        if (enemy.id != id || enemy.state == EnemyState::Dead) {
            continue;
        }

        const float resolvedDamage =
            (std::max)(1.0f, rawDamage - enemy.definition.defense);
        const float healthBefore = enemy.health;
        enemy.health = (std::max)(0.0f, enemy.health - resolvedDamage);
        enemy.hitFlashRemainingSeconds = kEnemyHitFlashSeconds;
        if (enemy.health <= 0.0f) {
            MarkDead(enemy);
        }

        UpdateSnapshot(enemy, snapshots_[index]);
        result.applied = true;
        result.killed = enemy.state == EnemyState::Dead;
        result.appliedDamage = healthBefore - enemy.health;
        result.remainingHealth = enemy.health;
        return result;
    }
    return result;
}

bool EnemySystem::Kill(const EnemyId id) {
    for (std::size_t index = 0; index < enemies_.size(); ++index) {
        RuntimeEnemy& enemy = enemies_[index];
        if (enemy.id != id || enemy.state == EnemyState::Dead) {
            continue;
        }
        enemy.health = 0.0f;
        enemy.hitFlashRemainingSeconds = kEnemyHitFlashSeconds;
        MarkDead(enemy);
        UpdateSnapshot(enemy, snapshots_[index]);
        return true;
    }
    return false;
}

void EnemySystem::MarkDead(RuntimeEnemy& enemy) {
    // An attack event can be emitted by Update and then the same enemy can be
    // killed by the player's shot before Game consumes this frame's queue.
    // Dead enemies must not deal that already-queued damage or spawn a bullet.
    std::erase_if(
        attackEvents_,
        [&enemy](const EnemyAttackEvent& event) {
            return event.enemyId == enemy.id;
        });
    SetState(enemy, EnemyState::Dead);
    enemy.attackCooldownSeconds = 0.0f;
    enemy.repathElapsedSeconds = 0.0f;
    enemy.stuckElapsedSeconds = 0.0f;
    enemy.navigationPurpose = NavigationPurpose::None;
    enemy.path.clear();
    enemy.nextWaypointIndex = 0;
    enemy.lastPlayerCell.reset();
}

void EnemySystem::RefreshSnapshots() {
    snapshots_.resize(enemies_.size());
    for (std::size_t index = 0; index < enemies_.size(); ++index) {
        UpdateSnapshot(enemies_[index], snapshots_[index]);
    }
}

void EnemySystem::UpdateSnapshot(const RuntimeEnemy& enemy, EnemySnapshot& snapshot) const {
    snapshot.id = enemy.id;
    snapshot.definitionId = enemy.definition.id;
    snapshot.kind = enemy.kind;
    snapshot.state = enemy.state;
    snapshot.position = enemy.position;
    snapshot.collisionRadius = enemy.definition.hitboxRadius;
    snapshot.hitboxHeight = enemy.definition.hitboxHeight;
    snapshot.health = enemy.health;
    snapshot.maxHealth = enemy.definition.maxHealth;
    snapshot.defense = enemy.definition.defense;
    snapshot.hitFlashRemainingSeconds = enemy.hitFlashRemainingSeconds;
    snapshot.stateElapsedSeconds = enemy.stateElapsedSeconds;
    snapshot.yawRadians = enemy.yawRadians;
    snapshot.pose = enemy.animation.CurrentPose();
    snapshot.body = {{enemy.position.x,0,enemy.position.z},enemy.definition.hitboxHeight,enemy.definition.hitboxRadius};
    snapshot.hurtboxes = enemy.state == EnemyState::Dead ? std::vector<EnemyHurtbox>{} :
        BuildEnemyHurtboxes(*enemy.definition.rig, snapshot.pose, enemy.position, enemy.yawRadians);
    snapshot.attackShape = enemy.attackShape;
}

} // namespace fps
