#include <doctest/doctest.h>

#include "RetroFPS/Pvp/ShotQuery.hpp"
#include "RetroFPS/Pvp/PlayerHitModel.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
fps::pvp::Arena ShotArena() {
    fps::pvp::Arena arena;
    arena.eyeHeight = 1;
    arena.bodyHeight = 2;
    arena.radius = .25F;
    return arena;
}
constexpr float HalfPi = std::numbers::pi_v<float> / 2;
}

TEST_CASE("PvP shot starts at authoritative eye height and excludes the shooter") {
    using namespace fps::pvp;
    const auto arena = ShotArena();
    const PlayerState shooter{7, {0, 0, 0}};
    const std::array players{shooter, PlayerState{8, {0, 0, 5}}};
    const auto hit = QueryShot(arena, shooter, 0, 0, players, 100);
    CHECK(hit.kind == ShotHitKind::Player);
    CHECK(hit.targetId == 8);
    CHECK(hit.distance == doctest::Approx(4.75));
    const auto miss = QueryShot(arena, shooter, HalfPi, 0, players, 100);
    CHECK(miss.kind == ShotHitKind::Miss);
    CHECK(miss.targetId == 0);
    CHECK(miss.distance == 100);
    CHECK(QueryShot(arena, shooter, 0, 0, std::span{players}.first(1), 100).kind ==
        ShotHitKind::Miss);
}

TEST_CASE("PvP shot range includes the contact boundary and selects the nearest target") {
    using namespace fps::pvp;
    const auto arena = ShotArena();
    const PlayerState shooter{1, {0, 0, 0}};
    const std::array players{PlayerState{2, {0, 0, 9}}, PlayerState{3, {0, 0, 5}}};
    CHECK(QueryShot(arena, shooter, 0, 0, players, 4.74F).kind == ShotHitKind::Miss);
    const auto hit = QueryShot(arena, shooter, 0, 0, players, 4.75F);
    CHECK(hit.kind == ShotHitKind::Player);
    CHECK(hit.targetId == 3);
    CHECK(hit.distance == 4.75F);
    CHECK(QueryShot(arena, shooter, 0, 0, players, 0).kind == ShotHitKind::Miss);
}

TEST_CASE("PvP shot respects wall occlusion and equal-distance world priority") {
    using namespace fps::pvp;
    auto arena = ShotArena();
    const PlayerState shooter{1, {0, 0, 0}};
    const std::array players{PlayerState{2, {0, 0, 5}}};
    arena.walls = {{{-1, 0, 3}, {1, 3, 4}}};
    auto hit = QueryShot(arena, shooter, 0, 0, players, 100);
    CHECK(hit.kind == ShotHitKind::World);
    CHECK(hit.targetId == 0);
    CHECK(hit.distance == 3);
    arena.walls.front() = {{-1, 0, 4.75F}, {1, 3, 6}};
    hit = QueryShot(arena, shooter, 0, 0, players, 100);
    CHECK(hit.kind == ShotHitKind::World);
    CHECK(hit.distance == 4.75F);
    arena.walls.front() = {{-1, 0, 6}, {1, 3, 7}};
    CHECK(QueryShot(arena, shooter, 0, 0, players, 100).kind == ShotHitKind::Player);
    arena.walls.front() = {{-1, 0, -.1F}, {1, 3, .1F}};
    hit = QueryShot(arena, shooter, 0, 0, players, 100);
    CHECK(hit.kind == ShotHitKind::World);
    CHECK(hit.distance == 0);
}

TEST_CASE("PvP floor blocks downward aim while upward aim reaches elevated targets") {
    using namespace fps::pvp;
    auto arena = ShotArena();
    const PlayerState shooter{1, {0, 0, 0}};
    auto hit = QueryShot(arena, shooter, 0, HalfPi / 2, {}, 100);
    CHECK(hit.kind == ShotHitKind::World);
    CHECK(hit.distance == doctest::Approx(std::sqrt(2.0F)));
    CHECK(QueryShot(arena, shooter, 0, HalfPi / 2, {}, 1.4F).kind == ShotHitKind::Miss);
    CHECK(QueryShot(arena, shooter, 0, -HalfPi / 2, {}, 100).kind == ShotHitKind::Miss);
    const std::array elevated{PlayerState{2, {0, 4, 4}}};
    hit = QueryShot(arena, shooter, 0, -HalfPi / 2, elevated, 100);
    CHECK(hit.kind == ShotHitKind::Player);
    CHECK(hit.targetId == 2);
    const PlayerState elevatedShooter{1, {0, 4, 0}};
    const std::array grounded{PlayerState{2, {0, 0, 4}}};
    CHECK(QueryShot(arena, elevatedShooter, 0, HalfPi / 2, grounded, 100).kind ==
        ShotHitKind::Player);
    // Aim below the floor cannot hit a synthetic below-floor target.
    const std::array belowFloor{PlayerState{2, {0, -5, 5}}};
    CHECK(QueryShot(arena, shooter, 0, HalfPi / 2, belowFloor, 100).kind ==
        ShotHitKind::World);
}

TEST_CASE("PvP capsules use rounded caps rather than full height boxes") {
    using namespace fps::pvp;
    auto arena = ShotArena();
    const PlayerState shooter{1, {0, 0, 0}};
    const std::array players{PlayerState{2, {.24F, 0, 5}}};
    arena.eyeHeight = 1;
    CHECK(QueryShot(arena, shooter, 0, 0, players, 100).kind == ShotHitKind::Player);
    arena.eyeHeight = 1.99F;
    CHECK(QueryShot(arena, shooter, 0, 0, players, 100).kind == ShotHitKind::Miss);
    arena.eyeHeight = .01F;
    CHECK(QueryShot(arena, shooter, 0, 0, players, 100).kind == ShotHitKind::Miss);
}

TEST_CASE("PvP equal-distance players select the lowest id in either input order") {
    using namespace fps::pvp;
    const auto arena = ShotArena();
    const PlayerState shooter{1, {0, 0, 0}};
    const std::array forward{PlayerState{9, {0, 0, 5}}, PlayerState{2, {0, 0, 5}}};
    const std::array reverse{forward.back(), forward.front()};
    CHECK(QueryShot(arena, shooter, 0, 0, forward, 100).targetId == 2);
    CHECK(QueryShot(arena, shooter, 0, 0, reverse, 100).targetId == 2);
}

TEST_CASE("PvP shot angles normalize and clamp like authoritative movement") {
    using namespace fps::pvp;
    const auto arena = ShotArena();
    const PlayerState shooter{1, {0, 0, 0}};
    const std::array players{PlayerState{2, {5, 0, 0}}};
    CHECK(QueryShot(arena, shooter, HalfPi, 0, players, 100).targetId == 2);
    CHECK(QueryShot(arena, shooter, HalfPi + 2 * std::numbers::pi_v<float>, 0,
        players, 100).targetId == 2);
    const auto clamped = QueryShot(arena, shooter, 0, HalfPi, {}, 100);
    const auto boundary = QueryShot(arena, shooter, 0, MovementMaximumPitch, {}, 100);
    CHECK(clamped.kind == ShotHitKind::World);
    CHECK(clamped.distance == boundary.distance);
}

TEST_CASE("PvP shot rejects invalid angles range and origin even in an empty arena") {
    using namespace fps::pvp;
    const auto arena = ShotArena();
    PlayerState shooter{1, {0, 0, 0}};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (const float yaw : {nan, infinity, 1.0e6F + 1})
        CHECK_THROWS_AS(static_cast<void>(QueryShot(arena, shooter, yaw, 0, {}, 100)), std::invalid_argument);
    for (const float pitch : {nan, infinity, HalfPi + .01F})
        CHECK_THROWS_AS(static_cast<void>(QueryShot(arena, shooter, 0, pitch, {}, 100)), std::invalid_argument);
    for (const float range : {nan, infinity, -1.F})
        CHECK_THROWS_AS(static_cast<void>(QueryShot(arena, shooter, 0, 0, {}, range)), std::invalid_argument);
    shooter.position.x = nan;
    CHECK_THROWS_AS(static_cast<void>(QueryShot(arena, shooter, 0, 0, {}, 100)), std::invalid_argument);
}

TEST_CASE("PvP dead capsules do not occlude a living target behind them") {
    using namespace fps::pvp;
    const auto arena = ShotArena();
    const PlayerState shooter{1, {0, 0, 0}};
    PlayerState dead{2, {0, 0, 3}};
    dead.lifeState = LifeState::Dead;
    const std::array players{dead, PlayerState{3, {0, 0, 5}}};
    const auto hit = QueryShot(arena, shooter, 0, 0, players, 100);
    CHECK(hit.kind == ShotHitKind::Player);
    CHECK(hit.targetId == 3);
    CHECK(hit.distance == 4.75F);
    CHECK(QueryShot(arena, shooter, 0, 0, std::span{players}.first(1), 100).kind == ShotHitKind::Miss);
}

// v8-product-hitbox-shot S2 (J3): shapes along one ray. The shooter at the
// origin looks along +z from eye height 1.
namespace {
fps::pvp::PlayerWorldHitShape Sphere(fps::pvp::HitPart part, float z, float y = 1, float radius = .2F) {
    return {part, Engine::Math::Capsule{{0, y, z}, {0, y, z}, radius}};
}
}

TEST_CASE("PvP a target's nearest shape decides its hit and its part") {
    using namespace fps::pvp;
    const auto arena = ShotArena();
    const PlayerState shooter{7, {0, 0, 0}};
    // A leg in front of the body on the ray: the leg is hit first, in either order.
    for (const bool reversed : {false, true}) {
        CAPTURE(reversed);
        std::vector shapes{Sphere(HitPart::Leg, 4), Sphere(HitPart::Body, 5)};
        if (reversed) std::swap(shapes[0], shapes[1]);
        const std::array targets{ShotTarget{8, shapes}};
        const auto hit = QueryShot(arena, DefaultStanceRules, shooter, 0, 0, targets, 100);
        CHECK(hit.kind == ShotHitKind::Player);
        CHECK(hit.targetId == 8);
        CHECK(hit.part == HitPart::Leg);
        CHECK(hit.distance == doctest::Approx(3.8));
    }
    // A shape off the ray is not hit; the one behind it is.
    std::vector shapes{Sphere(HitPart::Head, 4, 3), Sphere(HitPart::Body, 6)};
    const std::array targets{ShotTarget{8, shapes}};
    const auto hit = QueryShot(arena, DefaultStanceRules, shooter, 0, 0, targets, 100);
    CHECK(hit.part == HitPart::Body);
    CHECK(hit.distance == doctest::Approx(5.8));
    // No shape on the ray: the target is missed.
    std::vector off{Sphere(HitPart::Head, 4, 3)};
    const std::array missed{ShotTarget{8, off}};
    CHECK(QueryShot(arena, DefaultStanceRules, shooter, 0, 0, missed, 100).kind == ShotHitKind::Miss);
}

TEST_CASE("PvP shapes of one target at one distance take the higher part") {
    using namespace fps::pvp;
    const auto arena = ShotArena();
    const PlayerState shooter{7, {0, 0, 0}};
    // v8-product-hitbox-tie (a): Head over Body over Leg, whatever the shape order.
    const std::array<std::array<HitPart, 2>, 3> pairs{{{HitPart::Head, HitPart::Body}, {HitPart::Head, HitPart::Leg},
                                                       {HitPart::Body, HitPart::Leg}}};
    for (const auto& [higher, lower] : pairs) {
        for (const bool reversed : {false, true}) {
            CAPTURE(static_cast<int>(higher));
            CAPTURE(static_cast<int>(lower));
            CAPTURE(reversed);
            std::vector shapes{Sphere(higher, 5), Sphere(lower, 5)};
            if (reversed) std::swap(shapes[0], shapes[1]);
            const std::array targets{ShotTarget{8, shapes}};
            const auto hit = QueryShot(arena, DefaultStanceRules, shooter, 0, 0, targets, 100);
            CHECK(hit.part == higher);
        }
    }
    // An eye inside a shape is at distance 0; the higher part counts there too.
    std::vector inside{Sphere(HitPart::Leg, 0), Sphere(HitPart::Body, 0)};
    const std::array targets{ShotTarget{8, inside}};
    const auto hit = QueryShot(arena, DefaultStanceRules, shooter, 0, 0, targets, 100);
    CHECK(hit.distance == 0);
    CHECK(hit.part == HitPart::Body);
}

TEST_CASE("PvP targets at one distance keep the lowest id and the world before them") {
    using namespace fps::pvp;
    auto arena = ShotArena();
    const PlayerState shooter{7, {0, 0, 0}};
    // The lower id wins even with the lower part: parts rank only within one target.
    std::vector nine{Sphere(HitPart::Head, 5)}, eight{Sphere(HitPart::Leg, 5)};
    for (const bool reversed : {false, true}) {
        CAPTURE(reversed);
        std::vector targets{ShotTarget{9, nine}, ShotTarget{8, eight}};
        if (reversed) std::swap(targets[0], targets[1]);
        const auto hit = QueryShot(arena, DefaultStanceRules, shooter, 0, 0, targets, 100);
        CHECK(hit.targetId == 8);
        CHECK(hit.part == HitPart::Leg);
    }
    // A wall in front of the shapes occludes them; the equal-distance world
    // priority keeps its own test above.
    arena.walls.push_back({{-1, 0, 4.5F}, {1, 2, 6}});
    const std::array targets{ShotTarget{8, eight}};
    const auto hit = QueryShot(arena, DefaultStanceRules, shooter, 0, 0, targets, 100);
    CHECK(hit.kind == ShotHitKind::World);
    CHECK(hit.distance == doctest::Approx(4.5));
    // The shooter's own shapes are skipped.
    const std::array own{ShotTarget{7, eight}};
    arena.walls.clear();
    CHECK(QueryShot(arena, DefaultStanceRules, shooter, 0, 0, own, 100).kind == ShotHitKind::Miss);
}

TEST_CASE("PvP the synthetic model is the stance capsule judged as Body") {
    using namespace fps::pvp;
    const auto arena = ShotArena();
    for (const bool crouched : {false, true}) {
        PlayerState player{8, {1, 0, 5}};
        player.crouched = crouched;
        const CombatState combat;
        const PlayerAnimation animation;
        std::vector<PlayerWorldHitShape> shapes;
        SyntheticPlayerHitModel()->Shapes({arena, DefaultStanceRules, player, combat, animation, 1}, shapes);
        REQUIRE(shapes.size() == 1);
        CHECK(shapes[0].part == HitPart::Body);
        const auto& capsule = std::get<Engine::Collision::VerticalCapsule>(shapes[0].volume);
        CHECK(capsule.feet.x == 1);
        CHECK(capsule.feet.z == 5);
        CHECK(capsule.height == StanceBodyHeight(arena, DefaultStanceRules, crouched));
        CHECK(capsule.radius == arena.radius);
    }
}
