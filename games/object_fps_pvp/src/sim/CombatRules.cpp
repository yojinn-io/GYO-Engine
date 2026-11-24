#include "object_fps_pvp/sim/CombatRules.hpp"

#include "engine/math/linear/Vec3d.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace object_fps_pvp::sim {

namespace Math = GYO::Engine::Math;

std::uint32_t ShotDamage(const CombatRules& rules, const HitPart part, const std::uint32_t targetHp) noexcept {
    std::uint32_t permille = 0;
    switch (part) {
    case HitPart::Head: permille = rules.headPermille; break;
    case HitPart::Body: permille = rules.bodyPermille; break;
    case HitPart::Leg: permille = rules.legPermille; break;
    case HitPart::None: return 0;
    }
    const std::uint64_t scaled = std::uint64_t{rules.shotDamage} * permille / 1000;
    const std::uint64_t capped = std::min<std::uint64_t>(scaled, rules.maximumShotDamage);
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(capped, targetHp));
}

std::uint64_t TicksFor(const double seconds, const std::uint64_t stepNanoseconds) noexcept {
    if (stepNanoseconds == 0 || !(seconds > 0)) return 1;
    // A relative allowance of 1e-6 keeps a step length cut to whole nanoseconds (1/60 s) from adding a step.
    const double exact = seconds * 1e9 / static_cast<double>(stepNanoseconds);
    const double steps = std::ceil(exact * (1.0 - 1e-6));
    return steps < 1 ? 1 : static_cast<std::uint64_t>(steps);
}

Math::Vec3 ViewDirection(const float yaw, const float pitch) noexcept {
    const float cosinePitch = std::cos(pitch);
    return Math::Normalize(Math::Vec3{std::sin(yaw) * cosinePitch, -std::sin(pitch), std::cos(yaw) * cosinePitch});
}

int PartRank(const HitPart part) noexcept {
    switch (part) {
    case HitPart::Head: return 3;
    case HitPart::Body: return 2;
    case HitPart::Leg: return 1;
    case HitPart::None: return 0;
    }
    return 0;
}

ShotOutcome ChooseShotOutcome(const std::optional<float> world, const std::span<const PlayerHitCandidate> players,
                              const float range) noexcept {
    ShotOutcome closest{ShotHitKind::Miss, 0, range, HitPart::None};
    if (world && *world <= range) closest = {ShotHitKind::World, 0, *world, HitPart::None};
    for (const PlayerHitCandidate& candidate : players) {
        if (candidate.distance > range) continue;
        const bool nearer = candidate.distance < closest.distance;
        const bool tie = candidate.distance == closest.distance &&
                         (closest.kind == ShotHitKind::Miss ||
                          (closest.kind == ShotHitKind::Player && candidate.player < closest.player));
        if (nearer || tie) closest = {ShotHitKind::Player, candidate.player, candidate.distance, candidate.part};
    }
    return closest;
}

std::optional<std::size_t> ChooseSpawn(const std::span<const SpawnPoint> spawns, const std::span<const Math::Vec3> enemies,
                                       const std::function<bool(const SpawnPoint&)>& placeable) {
    std::optional<std::size_t> selected;
    double best = -1;
    for (std::size_t i = 0; i < spawns.size(); ++i) {
        if (!placeable(spawns[i])) continue;
        double nearest = (std::numeric_limits<double>::max)();
        for (const Math::Vec3& enemy : enemies) {
            nearest = std::min(nearest, Math::LengthSquared(Math::ToVec3d(spawns[i].position - enemy)));
        }
        // Strictly farther: equal distances keep the content order.
        if (!selected || nearest > best) {
            selected = i;
            best = nearest;
        }
    }
    return selected;
}

} // namespace object_fps_pvp::sim
