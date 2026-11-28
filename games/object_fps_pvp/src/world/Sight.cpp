#include "object_fps_pvp/world/Sight.hpp"

#include "object_fps_pvp/sim/CombatRules.hpp"
#include "object_fps_pvp/sim/BodySize.hpp"
#include "object_fps_pvp/sim/MovementRules.hpp"

namespace object_fps_pvp::world {

namespace P = GYO::Engine::Physics3d;

Math::Ray ShotRay(const GameRules& rules, const PlayerBody& body) {
    const float eye = sim::EyeHeight(rules.body, body.crouched != 0);
    return Math::Ray{body.feet + Math::Vec3{0.0F, eye, 0.0F}, sim::ViewDirection(body.yaw, body.pitch)};
}

std::optional<float> FloorAlong(const GameRules& rules, const Math::Ray& ray) {
    const auto floor = rules.arena.floor;
    if (!floor) return std::nullopt;
    if (ray.origin.y <= *floor) return 0.0F;
    if (ray.direction.y < 0.0F) return (ray.origin.y - *floor) / -ray.direction.y;
    return std::nullopt;
}

P::Capsule BodyCapsule(const GameRules& rules, const PlayerBody& body) {
    const float height = sim::BodyHeight(rules.body, body.crouched != 0);
    const float r = rules.body.radius;
    return P::Capsule{body.feet + Math::Vec3{0.0F, r, 0.0F}, body.feet + Math::Vec3{0.0F, height - r, 0.0F}, r};
}

const PlayerInput& Playable(const PlayerInput& input, const MatchInfo& info, const std::uint64_t tick) {
    static const PlayerInput none{};
    return info.phase == MatchPhase::Playing && tick >= info.startTick ? input : none;
}

} // namespace object_fps_pvp::world
