#include "RetroFPS/Collision/CombatCollision.hpp"
#include "RetroFPS/Collision/GridWorldCollision.hpp"

#include "engine/collision/Collision.hpp"

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

[[nodiscard]] bool IsFinite(const Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] float Length(const Float3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] Float3 AddScaled(
    const Float3 origin, const Float3 direction, const float distance) noexcept {
    return {
        origin.x + direction.x * distance,
        origin.y + direction.y * distance,
        origin.z + direction.z * distance,
    };
}

void ValidateQuery(
    const Float3 origin,
    const Float3 direction,
    const float maximumDistance,
    const float sweepRadius) {
    if (!IsFinite(origin) || !IsFinite(direction)) {
        throw std::invalid_argument("combat query vectors must be finite");
    }
    if (!std::isfinite(maximumDistance) || maximumDistance < 0.0f) {
        throw std::invalid_argument("combat query distance must be finite and non-negative");
    }
    if (!std::isfinite(sweepRadius) || sweepRadius < 0.0f) {
        throw std::invalid_argument("combat query sweep radius must be finite and non-negative");
    }
    const float directionLength = Length(direction);
    if (!std::isfinite(directionLength) || directionLength <= kEpsilon) {
        throw std::invalid_argument("combat query direction must be non-zero");
    }
}

[[nodiscard]] Float3 Normalize(const Float3 value) noexcept {
    const float length = Length(value);
    return {value.x / length, value.y / length, value.z / length};
}

[[nodiscard]] Engine::Collision::Float3 ToCollision(const Float3 value) noexcept {
    return {value.x, value.y, value.z};
}
[[nodiscard]] Engine::Collision::VerticalCapsule ToCollision(const VerticalCapsule& capsule) noexcept {
    return {{capsule.centerXZ.x, capsule.feetY, capsule.centerXZ.z}, capsule.height, capsule.radius};
}

} // namespace

std::optional<CombatHit> CombatCollision::Raycast(
    const GridMap& map,
    const WorldSettings& worldSettings,
    const Float3 origin,
    const Float3 direction,
    const float maximumDistance,
    const std::span<const CombatTarget> targets,
    const float sweepRadius) {
    ValidateQuery(origin, direction, maximumDistance, sweepRadius);
    if (!std::isfinite(worldSettings.cellSize) || worldSettings.cellSize <= 0.0f ||
        !std::isfinite(worldSettings.wallHeight) || worldSettings.wallHeight <= 0.0f) {
        throw std::invalid_argument("combat query world settings must be finite and positive");
    }
    const Float3 normalized = Normalize(direction);

    std::optional<CombatHit> closest;
    const auto consider = [&closest, origin, normalized](
                              const CombatHitKind kind,
                              const float distance,
                              const CombatTargetId targetId = 0, const std::string& region = {}) {
        if (!closest.has_value() || distance < closest->distance) {
            closest = CombatHit{kind, AddScaled(origin, normalized, distance), distance, targetId, region};
        }
    };

    for (const auto& box : BuildWorldCollisionBoxes(map,worldSettings)) {
        std::optional<float> distance;
        if(sweepRadius>0) {
            const Engine::Collision::VerticalCapsule sphere{
                {origin.x,origin.y-sweepRadius,origin.z},2*sweepRadius,sweepRadius};
            const auto contact=Engine::Collision::SweepVerticalCapsuleAgainstAabb(sphere,
                {normalized.x*maximumDistance,normalized.y*maximumDistance,normalized.z*maximumDistance},box);
            if(contact) distance=contact->fraction*maximumDistance;
        } else {
            distance=Engine::Collision::RaycastAabb(ToCollision(origin),ToCollision(normalized),maximumDistance,box);
        }
        if(distance) consider(CombatHitKind::Wall,*distance);
    }

    if (normalized.y < -kEpsilon && origin.y >= sweepRadius) {
        const float floorDistance = (sweepRadius - origin.y) / normalized.y;
        if (floorDistance >= 0.0f && floorDistance <= maximumDistance) {
            consider(CombatHitKind::Floor, floorDistance);
        }
    }

    for (const CombatTarget& target : targets) {
        const std::optional<float> distance = Engine::Collision::RaycastCapsule(
            ToCollision(origin), ToCollision(normalized), maximumDistance, target.capsule, sweepRadius);
        if (distance.has_value()) {
            consider(CombatHitKind::Target, *distance, target.id, target.region);
        }
    }
    return closest;
}

Float3 CombatCollision::ClampSegmentToWorld(
    const GridMap& map,
    const WorldSettings& worldSettings,
    const Float3 origin,
    const Float3 desiredEnd,
    const float clearance) {
    if (!IsFinite(origin) || !IsFinite(desiredEnd) ||
        !std::isfinite(clearance) || clearance <= 0.0f) {
        throw std::invalid_argument(
            "world segment endpoints must be finite and clearance positive");
    }
    const Float3 delta{
        desiredEnd.x - origin.x,
        desiredEnd.y - origin.y,
        desiredEnd.z - origin.z,
    };
    const float distance = Length(delta);
    if (!std::isfinite(distance)) {
        throw std::invalid_argument("world segment length must be finite");
    }
    if (distance <= kEpsilon) {
        return origin;
    }
    const auto hit = Raycast(map, worldSettings, origin, delta, distance);
    return hit ? AddScaled(origin, Normalize(delta),
                           (std::max)(0.0f, hit->distance - clearance))
               : desiredEnd;
}

std::optional<float> CombatCollision::RaycastCapsule(
    const Float3 origin, const Float3 direction, const float maximumDistance,
    const VerticalCapsule& capsule, const float sweepRadius) {
    return Engine::Collision::RaycastCapsule(
        ToCollision(origin), ToCollision(direction), maximumDistance, ToCollision(capsule), sweepRadius);
}

std::optional<float> CombatCollision::SweepSegmentAgainstCapsule(
    const Float3 start, const Float3 end, const float sweepRadius,
    const VerticalCapsule& capsule) {
    return Engine::Collision::SweepSphereAgainstCapsule(
        ToCollision(start), ToCollision(end), sweepRadius, ToCollision(capsule));
}

} // namespace fps
