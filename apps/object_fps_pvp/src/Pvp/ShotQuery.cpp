#include "RetroFPS/Pvp/ShotQuery.hpp"
#include "RetroFPS/Pvp/PlayerHitModel.hpp"
#include "engine/math/geometry/Intersection.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <optional>
#include <variant>
#include <vector>
#include <stdexcept>

namespace fps::pvp {

ShotHit QueryShot(const Arena& arena, const PlayerState& shooter, float yaw,
    float pitch, std::span<const PlayerState> players, float range) {
    return QueryShot(arena, DefaultStanceRules, shooter, yaw, pitch, players, range);
}

namespace {
// The stance capsule the judgement used before hitboxes.
PlayerWorldHitShape StanceCapsule(const Arena& arena, const StanceRules& rules, const PlayerState& player) {
    return {HitPart::Body, Engine::Collision::VerticalCapsule{
        player.position, StanceBodyHeight(arena, rules, player.crouched), arena.radius}};
}

class SyntheticHitModel final : public PlayerHitModel {
public:
    void Shapes(const PlayerHitInput& input, std::vector<PlayerWorldHitShape>& output) const override {
        output.push_back(StanceCapsule(input.arena, input.stance, input.player));
    }
};

// v8-product-hitbox-tie (a): at one distance a target's higher part counts.
int PartRank(HitPart part) noexcept {
    switch (part) {
    case HitPart::Head: return 3;
    case HitPart::Body: return 2;
    case HitPart::Leg: return 1;
    default: return 0;
    }
}

std::optional<float> Raycast(const Engine::Math::Ray& ray, float range, const PlayerWorldHitShape& shape) {
    return std::visit([&](const auto& volume) { return Engine::Collision::RaycastCapsule(ray, range, volume); },
                      shape.volume);
}
} // namespace

std::shared_ptr<const PlayerHitModel> SyntheticPlayerHitModel() {
    static const auto model = std::make_shared<const SyntheticHitModel>();
    return model;
}

ShotHit QueryShot(const Arena& arena, const StanceRules& rules, const PlayerState& shooter, float yaw,
    float pitch, std::span<const PlayerState> players, float range) {
    std::vector<PlayerWorldHitShape> shapes;
    shapes.reserve(players.size());
    for (const auto& player : players)
        if (player.lifeState != LifeState::Dead) shapes.push_back(StanceCapsule(arena, rules, player));
    std::vector<ShotTarget> targets;
    std::size_t next = 0;
    for (const auto& player : players)
        if (player.lifeState != LifeState::Dead) targets.push_back({player.playerId, std::span(&shapes[next++], 1)});
    return QueryShot(arena, rules, shooter, yaw, pitch, targets, range);
}

ShotHit QueryShot(const Arena& arena, const StanceRules& rules, const PlayerState& shooter, float yaw,
    float pitch, std::span<const ShotTarget> targets, float range) {
    if (!ValidMovementCommand({1, 0, 0, yaw, pitch}) ||
        !std::isfinite(range) || range < 0) {
        throw std::invalid_argument("Invalid shot angles or range");
    }
    const Engine::Math::Vec3 origin{
        shooter.position.x, shooter.position.y + StanceEyeHeight(arena, rules, shooter.crouched), shooter.position.z};

    yaw = Engine::Math::WrapRadians(yaw);
    pitch = Engine::Math::Clamp(pitch, -MovementMaximumPitch, MovementMaximumPitch);
    const float cosinePitch = std::cos(pitch);
    const Engine::Math::Vec3 direction = Engine::Math::Normalize(Engine::Math::Vec3{
        std::sin(yaw) * cosinePitch, -std::sin(pitch), std::cos(yaw) * cosinePitch});
    // The direction comes from validated angles; the origin may not be finite.
    if (!Engine::Collision::IsValid(Engine::Math::Ray{origin, direction}))
        throw std::invalid_argument("Invalid shot origin");

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
    // The nearest wall; the linear scan replaced on "<=", so the highest index
    // wins a tie. Every wall gives the same World result, so only the distance
    // (and "<=" against the floor) decides.
    if (const auto wall = arena.WallGrid().RaycastNearest({origin, direction}, range,
                                                          Engine::Collision::TieBreak::HighestIndex);
        wall && wall->distance <= closest.distance)
        closest = {ShotHitKind::World, 0, wall->distance};
    const Engine::Math::Ray ray{origin, direction};
    for (const auto& target : targets) {
        if (target.playerId == shooter.playerId) continue;
        // The target's nearest shape; at one distance the higher part.
        std::optional<float> distance;
        HitPart part{HitPart::Unspecified};
        for (const auto& shape : target.shapes) {
            const auto along = Raycast(ray, range, shape);
            if (along && (!distance || *along < *distance ||
                          (*along == *distance && PartRank(shape.part) > PartRank(part)))) {
                distance = along;
                part = shape.part;
            }
        }
        if (!distance) continue;
        if (*distance < closest.distance || (*distance == closest.distance &&
            (closest.kind == ShotHitKind::Miss ||
             (closest.kind == ShotHitKind::Player && target.playerId < closest.targetId)))) {
            closest = {ShotHitKind::Player, target.playerId, *distance, part};
        }
    }
    return closest;
}

} // namespace fps::pvp
