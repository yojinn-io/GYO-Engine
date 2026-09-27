#include <doctest/doctest.h>

#include "RetroFPS/Pvp/ShotQuery.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

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
