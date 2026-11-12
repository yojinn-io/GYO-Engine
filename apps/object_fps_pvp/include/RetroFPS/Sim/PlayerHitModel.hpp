#pragma once

#include "RetroFPS/Sim/Combat.hpp"
#include "RetroFPS/Sim/Movement.hpp"
#include "RetroFPS/Sim/PlayerAnimation.hpp"
#include "engine/collision/Collision.hpp"
#include "engine/math/geometry/Capsule.hpp"

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

namespace fps::pvp {
inline namespace sim {

// One hit shape of a target in the world (v8-product-hitbox-shot): the part it
// judges and its volume. A sphere is a capsule with equal ends.
struct PlayerWorldHitShape final {
    HitPart part{HitPart::Body};
    std::variant<Engine::Collision::VerticalCapsule, Engine::Math::Capsule> volume;
};

// What a model reads to place one target's shapes at one tick: the Match's
// authority values of that tick, never presentation state.
struct PlayerHitInput final {
    const Arena& arena;
    const StanceRules& stance;
    const PlayerState& player;
    const CombatState& combat;
    const PlayerAnimation& animation;
    std::uint64_t tick{};
};

// Where a target can be hit (v8-product-hitbox-shot-boundary (a)): the Match
// asks it once a tick for every living target (v8-product-hitbox-tick-pose (a)).
// The product's posed model lives in player_content; the domain only knows
// this interface, so its dependencies do not change.
class PlayerHitModel {
public:
    virtual ~PlayerHitModel() = default;
    // Appends the target's shapes. Throws when it cannot place them.
    virtual void Shapes(const PlayerHitInput& input, std::vector<PlayerWorldHitShape>& output) const = 0;
};

// The judgement before hitboxes and PvpMatch's default: one vertical capsule
// of the stance height and the arena radius, judged Body
// (v8-product-hitbox-test-model (a)). For unit tests and the authority digest,
// whose results then do not move with the player content; the Match passes
// the posed model.
[[nodiscard]] std::shared_ptr<const PlayerHitModel> SyntheticPlayerHitModel();

} // namespace sim
} // namespace fps::pvp
