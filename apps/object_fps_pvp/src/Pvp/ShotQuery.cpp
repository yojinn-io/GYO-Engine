#include "RetroFPS/Pvp/ShotQuery.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace fps::pvp {

ShotHit QueryShot(const Arena& arena, const PlayerState& shooter, float yaw,
    float pitch, std::span<const PlayerState> players, float range) {
    if (!ValidMovementCommand({1, 0, 0, yaw, pitch}) ||
        !std::isfinite(range) || range < 0) {
        throw std::invalid_argument("Invalid shot angles or range");
    }
    const Engine::Collision::Float3 origin{
        shooter.position.x, shooter.position.y + arena.eyeHeight, shooter.position.z};
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
        throw std::invalid_argument("Invalid shot origin");

    yaw = std::remainder(yaw, 2 * std::numbers::pi_v<float>);
    pitch = std::clamp(pitch, -MovementMaximumPitch, MovementMaximumPitch);
    const float cosinePitch = std::cos(pitch);
    Engine::Collision::Float3 direction{
        std::sin(yaw) * cosinePitch, -std::sin(pitch), std::cos(yaw) * cosinePitch};
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
        direction.z * direction.z);
    direction = {direction.x / length, direction.y / length, direction.z / length};

    ShotHit closest{ShotHitKind::Miss, 0, range};
    // The arena floor bounds a solid half-space; starting on/under it is overlap.
    if (origin.y <= 0) {
        closest = {ShotHitKind::World, 0, 0};
    } else if (direction.y < 0) {
        const float distance = -origin.y / direction.y;
        if (distance <= range) closest = {ShotHitKind::World, 0, distance};
    }
    for (const auto& wall : arena.walls) {
        const auto distance = Engine::Collision::RaycastAabb(origin, direction, range, wall);
        if (distance && *distance <= closest.distance)
            closest = {ShotHitKind::World, 0, *distance};
    }
    for (const auto& player : players) {
        if (player.playerId == shooter.playerId || player.lifeState == LifeState::Dead) continue;
        const Engine::Collision::VerticalCapsule capsule{
            {player.position.x, player.position.y, player.position.z},
            arena.bodyHeight, arena.radius};
        const auto distance = Engine::Collision::RaycastCapsule(origin, direction, range, capsule);
        if (!distance) continue;
        if (*distance < closest.distance || (*distance == closest.distance &&
            (closest.kind == ShotHitKind::Miss ||
             (closest.kind == ShotHitKind::Player && player.playerId < closest.targetId)))) {
            closest = {ShotHitKind::Player, player.playerId, *distance};
        }
    }
    return closest;
}

} // namespace fps::pvp
