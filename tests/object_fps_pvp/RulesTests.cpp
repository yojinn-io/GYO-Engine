// The movement, stance and combat rules that need no World (sim/MovementRules.hpp, sim/CombatRules.hpp): the jump
// reaches the arena's height, the fall stops at the floor, the stance heights, the damage of each part, the steps of
// a duration, the view direction, the shot's tie rules and the spawn choice.
#include <doctest/doctest.h>

#include "object_fps_pvp/sim/CombatRules.hpp"
#include "object_fps_pvp/sim/MovementRules.hpp"

#include <cmath>
#include <numbers>
#include <vector>

namespace {
using namespace object_fps_pvp::sim;
namespace Math = GYO::Engine::Math;

ArenaContent Arena() {
    ArenaContent arena;
    arena.id = "test";
    arena.width = 20;
    arena.depth = 20;
    return arena;
}
} // namespace

TEST_CASE("PvP movement rules: a jump peaks at the arena's height whatever the step") {
    const ArenaContent arena = Arena();
    for (const double step : {1.0 / 64, 1.0 / 60, 1.0 / 30}) {
        CAPTURE(step);
        float y = 0;
        float v = JumpSpeed(arena);
        float peak = 0;
        int steps = 0;
        do {
            const VerticalStep next = Fall(y, v, arena.gravity, step);
            y = next.y;
            v = next.velocity;
            peak = std::max(peak, y);
            ++steps;
        } while (y > 0 && steps < 1000);
        CHECK(y == 0.0F);
        CHECK(v == 0.0F);
        CHECK(peak <= arena.jumpHeight + 1e-4F);
        // The analytic fall reaches the height within one step's worth of the top of the arc.
        CHECK(peak > arena.jumpHeight - 0.5F * arena.gravity * static_cast<float>(step * step) - 1e-4F);
    }
}

TEST_CASE("PvP movement rules: the fall is the analytic constant acceleration and the floor stops it") {
    const VerticalStep step = Fall(2.0F, 1.0F, 18.0F, 0.25);
    CHECK(step.y == doctest::Approx(2.0 + 0.25 - 0.5 * 18 * 0.0625));
    CHECK(step.velocity == doctest::Approx(1.0 - 4.5));
    const VerticalStep landed = Fall(0.1F, -5.0F, 18.0F, 0.1);
    CHECK(landed.y == 0.0F);
    CHECK(landed.velocity == 0.0F);
}

TEST_CASE("PvP movement rules: the stance heights and speeds") {
    const ArenaContent arena = Arena();
    const StanceRules rules;
    CHECK(BodyHeight(arena, rules, false) == arena.bodyHeight);
    CHECK(BodyHeight(arena, rules, true) == rules.crouchBodyHeight);
    CHECK(EyeHeight(arena, rules, true) == rules.crouchEyeHeight);
    CHECK(MoveSpeed(arena, rules, true) == arena.movementSpeed * rules.crouchSpeedScale);
    CHECK(StanceFits(arena, rules));
    StanceRules tall = rules;
    tall.crouchBodyHeight = 2.0F;
    CHECK_FALSE(StanceFits(arena, tall));
    StanceRules thin = rules;
    thin.crouchBodyHeight = 0.4F;
    thin.crouchEyeHeight = 0.3F;
    CHECK_FALSE(StanceFits(arena, thin));
    std::string error;
    const auto parsed = ParseStanceRules(R"({"version": 1, "crouch_body_height": 1.1, "crouch_eye_height": 0.9, "crouch_speed_scale": 0.4, "sprint_speed_scale": 1})",
                                         error);
    REQUIRE_MESSAGE(parsed.has_value(), error);
    CHECK(parsed->crouchBodyHeight == 1.1F);
    CHECK_FALSE(ParseStanceRules(R"({"version": 2})", error).has_value());
}

TEST_CASE("PvP combat rules: the damage of each part") {
    const CombatRules rules;
    CHECK(ShotDamage(rules, HitPart::Body, rules.maximumHp) == 25'000);
    CHECK(ShotDamage(rules, HitPart::Head, rules.maximumHp) == 50'000);
    CHECK(ShotDamage(rules, HitPart::Leg, rules.maximumHp) == 18'750);
    CHECK(ShotDamage(rules, HitPart::None, rules.maximumHp) == 0);
    // At most maximumShotDamage, and at most what the target has left.
    CombatRules strong = rules;
    strong.headPermille = 5000;
    CHECK(ShotDamage(strong, HitPart::Head, rules.maximumHp) == rules.maximumShotDamage);
    CHECK(ShotDamage(rules, HitPart::Head, 10'000) == 10'000);
    CHECK(DisplayPoints(1) == 1);
    CHECK(DisplayPoints(100'000) == 100);
    CHECK(DisplayPoints(75'001) == 76);
}

TEST_CASE("PvP combat rules: a duration in steps rounds up and is at least one") {
    constexpr std::uint64_t step64 = 15'625'000;  // 1/64 s
    CHECK(TicksFor(1.5, step64) == 96);
    CHECK(TicksFor(1.0 / 6.0, step64) == 11);  // 10.67 steps
    CHECK(TicksFor(3.0, step64) == 192);
    CHECK(TicksFor(0.0, step64) == 1);
    CHECK(TicksFor(1.0, 1'000'000'000 / 60) == 60);
}

TEST_CASE("PvP combat rules: the view looks at +Z at yaw 0, turns toward +X, and a positive pitch looks down") {
    const Math::Vec3 ahead = ViewDirection(0, 0);
    CHECK(ahead.z == doctest::Approx(1));
    const Math::Vec3 right = ViewDirection(std::numbers::pi_v<float> / 2, 0);
    CHECK(right.x == doctest::Approx(1));
    CHECK(right.z == doctest::Approx(0).epsilon(1e-6));
    const Math::Vec3 down = ViewDirection(0, 0.5F);
    CHECK(down.y < 0);
    CHECK(Math::Length(down) == doctest::Approx(1));
}

TEST_CASE("PvP combat rules: the shot's outcome and its ties") {
    const float range = 100;
    // Nothing within range: a miss at the range.
    ShotOutcome miss = ChooseShotOutcome(std::nullopt, {}, range);
    CHECK(miss.kind == ShotHitKind::Miss);
    CHECK(miss.distance == range);
    // The nearest wins.
    const std::vector<PlayerHitCandidate> two{{7, 5.0F, HitPart::Body}, {3, 4.0F, HitPart::Leg}};
    ShotOutcome nearest = ChooseShotOutcome(10.0F, two, range);
    CHECK(nearest.kind == ShotHitKind::Player);
    CHECK(nearest.player == 3);
    CHECK(nearest.part == HitPart::Leg);
    // A wall nearer than the players stops the shot.
    CHECK(ChooseShotOutcome(3.0F, two, range).kind == ShotHitKind::World);
    // A wall at the same distance as a player wins.
    CHECK(ChooseShotOutcome(4.0F, two, range).kind == ShotHitKind::World);
    // Players at one distance: the lowest id, whatever the order.
    const std::vector<PlayerHitCandidate> tied{{9, 6.0F, HitPart::Body}, {4, 6.0F, HitPart::Body}, {6, 6.0F, HitPart::Body}};
    CHECK(ChooseShotOutcome(std::nullopt, tied, range).player == 4);
    // Beyond the range: not hit.
    const std::vector<PlayerHitCandidate> far{{1, 150.0F, HitPart::Body}};
    CHECK(ChooseShotOutcome(std::nullopt, far, range).kind == ShotHitKind::Miss);
    CHECK(PartRank(HitPart::Head) > PartRank(HitPart::Body));
    CHECK(PartRank(HitPart::Body) > PartRank(HitPart::Leg));
}

TEST_CASE("PvP combat rules: the spawn farthest from the nearest enemy, ties in content order") {
    const std::vector<SpawnPoint> spawns{{{1, 0, 1}, 0}, {{19, 0, 19}, 0}, {{1, 0, 19}, 0}, {{19, 0, 1}, 0}};
    const auto all = [](const SpawnPoint&) { return true; };
    // No enemy: the first.
    CHECK(ChooseSpawn(spawns, {}, all) == std::optional<std::size_t>{0});
    // An enemy near the first: the opposite corner.
    const std::vector<Math::Vec3> enemy{{2, 0, 2}};
    CHECK(ChooseSpawn(spawns, enemy, all) == std::optional<std::size_t>{1});
    // An enemy in the middle: every corner at one distance, the first in content order.
    const std::vector<Math::Vec3> middle{{10, 0, 10}};
    CHECK(ChooseSpawn(spawns, middle, all) == std::optional<std::size_t>{0});
    // The farthest is taken: the next farthest.
    const auto notSecond = [&](const SpawnPoint& s) { return &s != &spawns[1]; };
    CHECK(ChooseSpawn(spawns, enemy, notSecond) == std::optional<std::size_t>{2});
    // None free.
    CHECK_FALSE(ChooseSpawn(spawns, enemy, [](const SpawnPoint&) { return false; }).has_value());
}
