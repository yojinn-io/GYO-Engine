#include "RetroFPS/Pvp/PlayerAnimation.hpp"

#include "engine/math/scalar/Scalar.hpp"

#include <cmath>

namespace fps::pvp {
namespace {

// A planar step below this is standing still (the presentation's threshold).
constexpr double StillDistance = 0.000001;

// Ticks of a span that must be a whole number of ticks within [1, limit];
// 0 when it is not.
std::uint32_t WholeTicks(double seconds, std::uint32_t limit) {
    const double ticks = seconds * AuthorityTickRate;
    const double whole = std::round(ticks);
    if (!std::isfinite(ticks) || whole < 1 || whole > limit || std::abs(ticks - whole) > 0.000001) return 0;
    return static_cast<std::uint32_t>(whole);
}

// The move weight's step recovery (w x span + 999) / 1000 holds to 1000 steps.
constexpr std::uint32_t MaxTransitionTicks = 1000;
// Jump spans of up to a minute.
constexpr std::uint32_t MaxJumpTicks = 60 * AuthorityTickRate;

PlayerAnimation Reset(const PlayerAnimationRules& rules, const PlayerState& now, double planarSpeed,
                      std::uint64_t tick) {
    PlayerAnimation value;
    value.playerId = now.playerId;
    value.lifeGeneration = now.lifeGeneration;
    value.jogWeightPermille = PlayerJogWeightPermille(rules, planarSpeed);
    // A reset never invents a liftoff or landing it did not observe.
    value.jumpPhase = now.grounded ? JumpPhase::Grounded : JumpPhase::Airborne;
    value.jumpPhaseTick = tick;
    return value;
}

} // namespace

std::optional<PlayerAnimationRules> MakePlayerAnimationRules(const PlayerAnimationSource& source,
                                                             std::string& error) {
    error.clear();
    for (const double value : {source.referenceSpeed, source.maxFrameDeltaSeconds, source.walkNativeSpeed,
                               source.jogNativeSpeed, source.walkClipSeconds, source.jogClipSeconds})
        if (!std::isfinite(value) || value <= 0) {
            error = "player animation rules require finite positive speeds, lengths and frame bound";
            return std::nullopt;
        }
    if (source.jogNativeSpeed <= source.walkNativeSpeed) {
        error = "player animation rules require jog faster than walk";
        return std::nullopt;
    }
    PlayerAnimationRules rules;
    rules.walkNativeSpeed = source.walkNativeSpeed;
    rules.jogNativeSpeed = source.jogNativeSpeed;
    // Each clip covers its native speed times its authored length per cycle.
    rules.walkCycleDistance = source.walkNativeSpeed * source.walkClipSeconds;
    rules.jogCycleDistance = source.jogNativeSpeed * source.jogClipSeconds;
    rules.teleportDistance = source.referenceSpeed * source.maxFrameDeltaSeconds;
    for (const double distance : {rules.walkCycleDistance, rules.jogCycleDistance, rules.teleportDistance})
        if (!std::isfinite(distance) || distance < 0.000001) {
            error = "player animation cycle and teleport distances must be finite and at least a micrometre";
            return std::nullopt;
        }
    rules.transitionTicks = WholeTicks(source.transitionSeconds, MaxTransitionTicks);
    rules.jumpStartTicks = WholeTicks(source.jumpStartSeconds, MaxJumpTicks);
    rules.jumpLandTicks = WholeTicks(source.jumpLandSeconds, MaxJumpTicks);
    if (!rules.transitionTicks || !rules.jumpStartTicks || !rules.jumpLandTicks) {
        error = "player animation spans must be whole numbers of ticks: transition 1..1000, jumps 1..3600";
        return std::nullopt;
    }
    return rules;
}

double PlayerJogRatio(const PlayerAnimationRules& rules, double planarSpeed) noexcept {
    return Engine::Math::Clamp((planarSpeed - rules.walkNativeSpeed) /
        (rules.jogNativeSpeed - rules.walkNativeSpeed), 0.0, 1.0);
}

std::uint32_t PlayerJogWeightPermille(const PlayerAnimationRules& rules, double planarSpeed) noexcept {
    return static_cast<std::uint32_t>(std::floor(1000 * PlayerJogRatio(rules, planarSpeed)));
}

double PlayerCycleDistance(const PlayerAnimationRules& rules, double jogRatio) noexcept {
    return rules.walkCycleDistance + (rules.jogCycleDistance - rules.walkCycleDistance) * jogRatio;
}

PlayerAnimation AdvancePlayerAnimation(const PlayerAnimationRules& rules,
    const std::optional<PlayerAnimation>& previous, const PlayerState* before, const PlayerState& now,
    std::uint64_t tick) {
    // The previous value must be the one advanced from `before`: same player,
    // same life, and a zero point not ahead of this tick.
    const bool samePlayer = previous && before && previous->playerId == now.playerId &&
        before->playerId == now.playerId && previous->lifeGeneration == before->lifeGeneration &&
        previous->jumpPhaseTick <= tick;
    // Death freezes the last value of the life (protocol-v9 §4.4).
    if (now.lifeState == LifeState::Dead && samePlayer && previous->lifeGeneration == now.lifeGeneration)
        return *previous;
    double dx = 0, dz = 0;
    if (before) {
        dx = static_cast<double>(now.position.x) - before->position.x;
        dz = static_cast<double>(now.position.z) - before->position.z;
    }
    const double distance = std::sqrt(dx * dx + dz * dz);
    const double planarSpeed = distance * AuthorityTickRate;
    // A new player, life or epoch starts at rest: a step across the boundary
    // is not this life's speed. A teleport inside one segment keeps its step,
    // as the Client's timeline did; a non-finite step starts at rest.
    if (!samePlayer || before->lifeGeneration != now.lifeGeneration || before->lifeState != now.lifeState ||
        before->movementEpoch != now.movementEpoch || !std::isfinite(distance))
        return Reset(rules, now, 0, tick);
    if (distance > rules.teleportDistance) return Reset(rules, now, planarSpeed, tick);

    auto value = *previous;
    const bool moving = distance > StillDistance;
    const bool backward = moving && dx * std::sin(now.yaw) + dz * std::cos(now.yaw) < -StillDistance;
    if (planarSpeed > 0) value.jogWeightPermille = PlayerJogWeightPermille(rules, planarSpeed);
    if (moving) {
        // The exact ratio of this tick's speed sets the cycle length; only the
        // stored weight is quantized. A still tick adds no phase.
        const double cycles = (backward ? -distance : distance) /
            PlayerCycleDistance(rules, PlayerJogRatio(rules, planarSpeed));
        const auto delta = static_cast<std::int64_t>(std::llround(cycles * 65536.0));
        value.gaitPhaseQ16 = static_cast<std::uint32_t>(static_cast<std::uint64_t>(value.gaitPhaseQ16) +
                                                        static_cast<std::uint64_t>(delta));
    }
    // The weight is 1000 x k / transition for k steps toward moving, so it
    // takes the same values as the presentation's float ramp, floored.
    const auto span = rules.transitionTicks;
    std::uint32_t steps = (value.moveWeightPermille * span + 999) / 1000;
    steps = moving ? Engine::Math::Min(span, steps + 1) : (steps ? steps - 1 : 0);
    value.moveWeightPermille = 1000 * steps / span;

    if (before->grounded && !now.grounded) {
        value.jumpPhase = now.verticalVelocity > 0 ? JumpPhase::Start : JumpPhase::Airborne;
        value.jumpPhaseTick = tick;
    } else if (!before->grounded && now.grounded) {
        value.jumpPhase = JumpPhase::Land;
        value.jumpPhaseTick = tick;
    }
    // The spans end in authority time; control never waits for them.
    if (value.jumpPhase == JumpPhase::Start && tick - value.jumpPhaseTick >= rules.jumpStartTicks) {
        value.jumpPhase = JumpPhase::Airborne;
        value.jumpPhaseTick += rules.jumpStartTicks;
    } else if (value.jumpPhase == JumpPhase::Land && tick - value.jumpPhaseTick >= rules.jumpLandTicks) {
        value.jumpPhase = JumpPhase::Grounded;
        value.jumpPhaseTick += rules.jumpLandTicks;
    }
    value.lifeGeneration = now.lifeGeneration;
    value.sprintWeightPermille = 0;
    return value;
}

} // namespace fps::pvp
