#include "RetroFPS/Pvp/ShotQuery.hpp"
#include "engine/math/geometry/Intersection.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <stdexcept>

namespace fps::pvp {

ShotHit QueryShot(const Arena& arena, const PlayerState& shooter, float yaw,
    float pitch, std::span<const PlayerState> players, float range) {
    if (!ValidMovementCommand({1, 0, 0, yaw, pitch}) ||
        !std::isfinite(range) || range < 0) {
        throw std::invalid_argument("Invalid shot angles or range");
    }
    const Engine::Math::Vec3 origin{
        shooter.position.x, shooter.position.y + arena.eyeHeight, shooter.position.z};
    if (!Engine::Math::IsFinite(origin)) throw std::invalid_argument("Invalid shot origin");

    yaw = Engine::Math::WrapRadians(yaw);
    pitch = Engine::Math::Clamp(pitch, -MovementMaximumPitch, MovementMaximumPitch);
    const float cosinePitch = std::cos(pitch);
    const Engine::Math::Vec3 direction = Engine::Math::Normalize(Engine::Math::Vec3{
        std::sin(yaw) * cosinePitch, -std::sin(pitch), std::cos(yaw) * cosinePitch});

    ShotHit closest{ShotHitKind::Miss, 0, range};
    // The arena floor bounds a solid half-space; starting on/under it is overlap.
    // Plane{} is the floor y = 0; only a downward ray from above it can hit.
    if (origin.y <= 0) {
        closest = {ShotHitKind::World, 0, 0};
    } else if (const auto floor =
                   Engine::Math::Intersect(Engine::Math::Ray{origin, direction}, Engine::Math::Plane{});
               floor && *floor <= range) {
        closest = {ShotHitKind::World, 0, *floor};
    }
    for (const auto& wall : arena.walls) {
        const auto distance = Engine::Collision::RaycastAabb({origin, direction}, range, wall);
        if (distance && *distance <= closest.distance)
            closest = {ShotHitKind::World, 0, *distance};
    }
    for (const auto& player : players) {
        if (player.playerId == shooter.playerId || player.lifeState == LifeState::Dead) continue;
        const Engine::Collision::VerticalCapsule capsule{player.position, arena.bodyHeight, arena.radius};
        const auto distance = Engine::Collision::RaycastCapsule({origin, direction}, range, capsule);
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
