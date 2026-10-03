#include "RetroFPS/Collision/CombatCollision.hpp"
#include "RetroFPS/Collision/GridWorldCollision.hpp"

#include "engine/collision/Collision.hpp"
#include "engine/math/geometry/Intersection.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include "RetroFPS/World/GridMap.hpp"
#include "RetroFPS/World/WorldSettings.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fps {
namespace {

constexpr float kEpsilon = 0.000001f;

void ValidateQuery(
    const Engine::Math::Vec3 origin,
    const Engine::Math::Vec3 direction,
    const float maximumDistance,
    const float sweepRadius) {
    if (!Engine::Math::IsFinite(origin) || !Engine::Math::IsFinite(direction)) {
        throw std::invalid_argument("combat query vectors must be finite");
    }
    if (!std::isfinite(maximumDistance) || maximumDistance < 0.0f) {
        throw std::invalid_argument("combat query distance must be finite and non-negative");
    }
    if (!std::isfinite(sweepRadius) || sweepRadius < 0.0f) {
        throw std::invalid_argument("combat query sweep radius must be finite and non-negative");
    }
    const float directionLength = Engine::Math::Length(direction);
    if (!std::isfinite(directionLength) || directionLength <= kEpsilon) {
        throw std::invalid_argument("combat query direction must be non-zero");
    }
}

[[nodiscard]] Engine::Collision::VerticalCapsule ToCollision(const VerticalCapsule& capsule) noexcept {
    return {{capsule.centerXZ.x, capsule.feetY, capsule.centerXZ.z}, capsule.height, capsule.radius};
}

} // namespace

std::optional<CombatHit> CombatCollision::Raycast(
    const GridMap& map,
    const WorldSettings& worldSettings,
    const Engine::Math::Vec3 origin,
    const Engine::Math::Vec3 direction,
    const float maximumDistance,
    const std::span<const CombatTarget> targets,
    const float sweepRadius) {
    ValidateQuery(origin, direction, maximumDistance, sweepRadius);
    if (!std::isfinite(worldSettings.cellSize) || worldSettings.cellSize <= 0.0f ||
        !std::isfinite(worldSettings.wallHeight) || worldSettings.wallHeight <= 0.0f) {
        throw std::invalid_argument("combat query world settings must be finite and positive");
    }
    const Engine::Math::Vec3 normalized = Engine::Math::Normalize(direction);

    std::optional<CombatHit> closest;
    const auto consider = [&closest, origin, normalized](
                              const CombatHitKind kind,
                              const float distance,
                              const CombatTargetId targetId = 0, const std::string& region = {}) {
        if (!closest.has_value() || distance < closest->distance) {
            closest = CombatHit{kind, origin + normalized * distance, distance, targetId, region};
        }
    };

    for (const auto& box : BuildWorldCollisionBoxes(map,worldSettings)) {
        std::optional<float> distance;
        if(sweepRadius>0) {
            const Engine::Collision::VerticalCapsule sphere{
                {origin.x,origin.y-sweepRadius,origin.z},2*sweepRadius,sweepRadius};
            const auto contact=Engine::Collision::SweepVerticalCapsuleAgainstAabb(sphere,
                normalized*maximumDistance,box);
            if(contact) distance=contact->fraction*maximumDistance;
        } else {
            distance=Engine::Collision::RaycastAabb({origin, normalized},maximumDistance,box);
        }
        if(distance) consider(CombatHitKind::Wall,*distance);
    }

    // The swept sphere's center meets the floor on the plane y = sweepRadius.
    if (normalized.y < -kEpsilon && origin.y >= sweepRadius) {
        const auto floorDistance = Engine::Math::Intersect(
            Engine::Math::Ray{origin, normalized}, Engine::Math::Plane{{0.0f, 1.0f, 0.0f}, sweepRadius});
        if (floorDistance && *floorDistance <= maximumDistance) {
            consider(CombatHitKind::Floor, *floorDistance);
        }
    }

    for (const CombatTarget& target : targets) {
        const std::optional<float> distance = Engine::Collision::RaycastCapsule({origin, normalized}, maximumDistance, target.capsule, sweepRadius);
        if (distance.has_value()) {
            consider(CombatHitKind::Target, *distance, target.id, target.region);
        }
    }
    return closest;
}

Engine::Math::Vec3 CombatCollision::ClampSegmentToWorld(
    const GridMap& map,
    const WorldSettings& worldSettings,
    const Engine::Math::Vec3 origin,
    const Engine::Math::Vec3 desiredEnd,
    const float clearance) {
    if (!Engine::Math::IsFinite(origin) || !Engine::Math::IsFinite(desiredEnd) ||
        !std::isfinite(clearance) || clearance <= 0.0f) {
        throw std::invalid_argument(
            "world segment endpoints must be finite and clearance positive");
    }
    const Engine::Math::Vec3 delta = desiredEnd - origin;
    const float distance = Engine::Math::Length(delta);
    if (!std::isfinite(distance)) {
        throw std::invalid_argument("world segment length must be finite");
    }
    if (distance <= kEpsilon) {
        return origin;
    }
    const auto hit = Raycast(map, worldSettings, origin, delta, distance);
    return hit ? origin + Engine::Math::Normalize(delta) * Engine::Math::Max(0.0f, hit->distance - clearance)
               : desiredEnd;
}

std::optional<float> CombatCollision::RaycastCapsule(
    const Engine::Math::Vec3 origin, const Engine::Math::Vec3 direction, const float maximumDistance,
    const VerticalCapsule& capsule, const float sweepRadius) {
    return Engine::Collision::RaycastCapsule({origin, direction}, maximumDistance, ToCollision(capsule), sweepRadius);
}

std::optional<float> CombatCollision::SweepSegmentAgainstCapsule(
    const Engine::Math::Vec3 start, const Engine::Math::Vec3 end, const float sweepRadius,
    const VerticalCapsule& capsule) {
    return Engine::Collision::SweepSphereAgainstCapsule({start, end}, sweepRadius, ToCollision(capsule));
}

} // namespace fps
