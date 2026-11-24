// The player's animation rules (sim/PlayerAnimation.hpp): the gait walks cycles at the speed, backward counts down,
// a teleport is not walked, the move eases in, the jump runs its phases, and the actions of a life are chosen in
// their order (death over everything, a hit over a reload over a shot), the aim between up and down.
#include <doctest/doctest.h>

#include "object_fps_pvp/sim/PlayerAnimation.hpp"

#include <cmath>

using namespace object_fps_pvp::sim;

namespace {
AnimationRules Rules() {
    AnimationRules rules;
    rules.walkCycleDistance = 1.0;
    rules.jogCycleDistance = 4.0;
    return rules;
}
AnimationInput At(const double seconds, const float z, const bool grounded = true) {
    AnimationInput input;
    input.seconds = seconds;
    input.feet = {0, 0, z};
    input.grounded = grounded;
    return input;
}
} // namespace

TEST_CASE("PvP animation: walking forward adds cycles, backward takes them away, a teleport adds none") {
    const AnimationRules rules = Rules();
    Locomotion state;
    Advance(state, At(0, 0), rules);
    for (int i = 1; i <= 64; ++i) Advance(state, At(i / 64.0, static_cast<float>(3.0 * i / 64.0)), rules);
    // 3 units at 3 units a second: the jog's share follows the speed, the cycle distance between walk and jog.
    const double jog = (3.0 - rules.walkNativeSpeed) / (rules.jogNativeSpeed - rules.walkNativeSpeed);
    CHECK(state.jogWeight == doctest::Approx(jog));
    CHECK(state.cycles == doctest::Approx(3.0 / (1.0 + 3.0 * jog)).epsilon(0.02));
    CHECK(state.moveWeight == doctest::Approx(1.0).epsilon(1e-3));
    const double forward = state.cycles;
    Advance(state, At(65 / 64.0, 3.0F - 3.0F / 64.0F), rules);
    CHECK(state.cycles < forward);
    const double before = state.cycles;
    Advance(state, At(66 / 64.0, 20.0F), rules);
    CHECK(state.cycles == before);
    // Standing: the move eases out.
    for (int i = 67; i < 100; ++i) Advance(state, At(i / 64.0, 20.0F), rules);
    CHECK(state.moveWeight < 0.01);
}

TEST_CASE("PvP animation: a jump starts, flies and lands, each phase from its moment") {
    const AnimationRules rules = Rules();
    Locomotion state;
    Advance(state, At(0, 0), rules);
    Advance(state, At(1.0, 0, false), rules);
    CHECK(state.jump == JumpPhase::Start);
    Advance(state, At(1.05, 0, false), rules);
    CHECK(state.jump == JumpPhase::Start);
    Advance(state, At(1.11, 0, false), rules);
    CHECK(state.jump == JumpPhase::Airborne);
    Advance(state, At(1.5, 0, true), rules);
    CHECK(state.jump == JumpPhase::Land);
    const ActionPose landing = ResolveActions(state, At(1.55, 0, true), rules);
    CHECK(landing.lower == LowerClip::JumpLand);
    CHECK(landing.lowerSeconds == doctest::Approx(0.5));
    Advance(state, At(1.61, 0, true), rules);
    CHECK(state.jump == JumpPhase::Grounded);
}

TEST_CASE("PvP animation: death over everything, a hit over a reload over a shot, only of this life; the aim") {
    const AnimationRules rules = Rules();
    Locomotion state;
    AnimationInput input = At(10.0, 0);
    input.lifeSeconds = 5.0;
    input.shotSeconds = 9.95;
    CHECK(ResolveActions(state, input, rules).upper == UpperClip::Shoot);
    input.reloadStart = 9.5;
    input.reloadEnd = 11.0;
    const ActionPose reloading = ResolveActions(state, input, rules);
    CHECK(reloading.upper == UpperClip::Reload);
    CHECK(reloading.upperProgress == doctest::Approx(0.5 / 1.5));
    input.hitSeconds = 9.9;
    input.hitPart = HitPart::Head;
    CHECK(ResolveActions(state, input, rules).upper == UpperClip::HitHead);
    // A hit of the last life does not play.
    input.lifeSeconds = 9.92;
    CHECK(ResolveActions(state, input, rules).upper == UpperClip::Shoot);
    input.dead = true;
    input.lifeSeconds = 9.0;
    const ActionPose dead = ResolveActions(state, input, rules);
    CHECK(dead.lower == LowerClip::Death);
    CHECK(dead.lowerSeconds == doctest::Approx(1.0));
    AnimationInput aim = At(1.0, 0);
    aim.pitch = -rules.aimUpRadians / 2;
    CHECK(ResolveActions(state, aim, rules).aim == doctest::Approx(-0.5));
    aim.pitch = rules.aimDownRadians * 2;
    CHECK(ResolveActions(state, aim, rules).aim == 1.0F);
}
