#pragma once

#include "RetroFPS/Math/Vector.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace fps {

class GridMap;
struct WorldSettings;

using CombatTargetId = std::uint64_t;

struct VerticalCapsule final {
    Float2 centerXZ{};
    float height = 0.0f;
    float radius = 0.0f;
    float feetY = 0.0f;
};

struct CombatTarget final {
    CombatTargetId id = 0;
    VerticalCapsule capsule{};
};

enum class CombatHitKind {
    Wall,
    Floor,
    Target,
};

struct CombatHit final {
    CombatHitKind kind = CombatHitKind::Wall;
    Float3 position{};
    float distance = 0.0f;
    CombatTargetId targetId = 0;
};

// Game world queries compose reusable GYO collision primitives.
// Grid movement remains in GridCollision; this class treats every solid tile as
// a wall-height AABB and characters as upright capsules.
class CombatCollision final {
public:
    [[nodiscard]] static std::optional<CombatHit> Raycast(
        const GridMap& map,
        const WorldSettings& worldSettings,
        Float3 origin,
        Float3 direction,
        float maximumDistance,
        std::span<const CombatTarget> targets = {},
        float sweepRadius = 0.0f);

    // Constrain an offset point to the first wall/floor along a segment from
    // a free-space origin. Clearance is measured back along that segment;
    // this does not enlarge geometry or ignore subsequent shot obstructions.
    [[nodiscard]] static Float3 ClampSegmentToWorld(
        const GridMap& map,
        const WorldSettings& worldSettings,
        Float3 origin,
        Float3 desiredEnd,
        float clearance = 0.001f);

    [[nodiscard]] static std::optional<float> RaycastCapsule(
        Float3 origin,
        Float3 direction,
        float maximumDistance,
        const VerticalCapsule& capsule,
        float sweepRadius = 0.0f);

    // Returns the normalized segment fraction [0, 1] of the first hit.
    [[nodiscard]] static std::optional<float> SweepSegmentAgainstCapsule(
        Float3 start,
        Float3 end,
        float sweepRadius,
        const VerticalCapsule& capsule);
};

} // namespace fps
