#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "engine/collision/Collision.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace Engine::Collision;

TEST_CASE("capsule queries use world feet and normalize ray direction") {
    const VerticalCapsule capsule{{0.0f, 0.6f, 0.0f}, 1.8f, 0.25f};
    const auto hit = RaycastCapsule({-1.0f, 1.2f, 0.0f}, {2.0f, 0.0f, 0.0f}, 3.0f, capsule);
    REQUIRE(hit);
    CHECK(*hit == doctest::Approx(0.75f));
    CHECK_FALSE(RaycastCapsule({-1.0f, 0.3f, 0.0f}, {1.0f, 0.0f, 0.0f}, 3.0f, capsule));
    const auto top = RaycastCapsule({0.0f, 4.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 5.0f, capsule);
    REQUIRE(top);
    CHECK(*top == doctest::Approx(1.6f));
    const auto overlap = RaycastCapsule({0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 2.0f, capsule);
    REQUIRE(overlap);
    CHECK(*overlap == 0.0f);
}

TEST_CASE("sphere sweeps report first fraction including stationary overlap") {
    const VerticalCapsule capsule{{0.0f, 0.6f, 0.0f}, 1.8f, 0.25f};
    const auto hit = SweepSphereAgainstCapsule({-1.0f, 1.2f, 0.0f}, {1.0f, 1.2f, 0.0f}, 0.05f, capsule);
    REQUIRE(hit);
    CHECK(*hit == doctest::Approx(0.35f));
    const auto stationary = SweepSphereAgainstCapsule({0.0f, 0.6f, 0.0f}, {0.0f, 0.6f, 0.0f}, 0.0f, capsule);
    REQUIRE(stationary);
    CHECK(*stationary == 0.0f);
    CHECK_FALSE(SweepSphereAgainstCapsule({0.0f, 0.1f, 0.0f}, {0.0f, 0.1f, 0.0f}, 0.0f, capsule));
    CHECK_FALSE(SweepSphereAgainstCapsule({-1.0f, 0.3f, 0.0f}, {1.0f, 0.3f, 0.0f}, 0.05f, capsule));
}

TEST_CASE("primitive queries reject malformed input") {
    const VerticalCapsule valid{{0.0f, 0.0f, 0.0f}, 1.8f, 0.25f};
    CHECK_THROWS_AS(static_cast<void>(RaycastCapsule({}, {}, 1.0f, valid)), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(SweepSphereAgainstCapsule({}, {}, -1.0f, valid)), std::invalid_argument);
    const VerticalCapsule bad{{0.0f, (std::numeric_limits<float>::quiet_NaN)(), 0.0f}, 1.8f, 0.25f};
    CHECK_THROWS_AS(static_cast<void>(SweepSphereAgainstCapsule({}, {}, 0.0f, bad)), std::invalid_argument);
    const VerticalCapsule tooShort{{}, 0.1f, 0.25f};
    CHECK_THROWS_AS(static_cast<void>(RaycastCapsule({}, {1.0f, 0.0f, 0.0f}, 1.0f, tooShort)), std::invalid_argument);
}

TEST_CASE("AABB ray reports normalized distance and immediate overlap") {
    const Aabb box{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    const auto hit = RaycastAabb({-1.0f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, 2.0f, box);
    REQUIRE(hit);
    CHECK(*hit == doctest::Approx(1.0f));
    const auto inside = RaycastAabb({0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 2.0f, box);
    REQUIRE(inside);
    CHECK(*inside == 0.0f);
    CHECK_FALSE(RaycastAabb({-1.0f, 2.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 2.0f, box));
    CHECK_THROWS_AS(static_cast<void>(RaycastAabb({}, {1.0f, 0.0f, 0.0f}, 1.0f, {{1.0f,0.0f,0.0f}, {}})), std::invalid_argument);
}

TEST_CASE("arbitrary capsules support rotated axes, end caps, and degenerate spheres") {
    const Capsule horizontal{{-1.0f, 2.0f, 0.0f}, {1.0f, 2.0f, 0.0f}, 0.25f};
    const auto side = RaycastCapsule({0.0f, 2.0f, -2.0f}, {0.0f, 0.0f, 4.0f}, 4.0f, horizontal);
    REQUIRE(side);
    CHECK(*side == doctest::Approx(1.75f));
    const auto end = RaycastCapsule({3.0f, 2.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 4.0f, horizontal);
    REQUIRE(end);
    CHECK(*end == doctest::Approx(1.75f));

    const Capsule tilted{{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, 0.5f};
    const auto perpendicular = RaycastCapsule({0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, 1.0f}, 4.0f, tilted);
    REQUIRE(perpendicular);
    CHECK(*perpendicular == doctest::Approx(1.5f));
    CHECK_FALSE(RaycastCapsule({1.0f, -1.0f, -2.0f}, {0.0f, 0.0f, 1.0f}, 4.0f, tilted));

    const Capsule sphere{{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 0.5f};
    const auto sphereHit = RaycastCapsule({-2.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 4.0f, sphere);
    REQUIRE(sphereHit);
    CHECK(*sphereHit == doctest::Approx(1.5f));
    CHECK_FALSE(RaycastCapsule({-2.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 1.49f, sphere));
}

TEST_CASE("arbitrary sphere sweeps handle nonunit segments, tangency and stationary overlap") {
    const Capsule c{{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 0.25f};
    const auto sweep = SweepSphereAgainstCapsule({0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, 2.0f}, 0.25f, c);
    REQUIRE(sweep);
    CHECK(*sweep == doctest::Approx(0.375f));
    const auto tangent = SweepSphereAgainstCapsule({0.0f, 0.5f, -2.0f}, {0.0f, 0.5f, 2.0f}, 0.25f, c);
    REQUIRE(tangent);
    CHECK(*tangent == doctest::Approx(0.5f));
    const auto stationary = SweepSphereAgainstCapsule({}, {}, 0.0f, c);
    REQUIRE(stationary);
    CHECK(*stationary == 0.0f);
    CHECK_FALSE(SweepSphereAgainstCapsule({0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 0.0f, c));
}

TEST_CASE("new arbitrary capsule queries agree with legacy upright queries") {
    const VerticalCapsule upright{{1.0f, 0.4f, -2.0f}, 1.8f, 0.25f};
    const Capsule general = ToCapsule(upright);
    CHECK(general.segmentStart.y == doctest::Approx(0.65f));
    CHECK(general.segmentEnd.y == doctest::Approx(1.95f));
    for (int height = -3; height <= 15; ++height) {
        const Float3 start{-3.0f, height * 0.2f, -2.0f};
        const Float3 end{3.0f, start.y, start.z};
        const auto legacy = SweepSphereAgainstCapsule(start, end, 0.1f, upright);
        const auto current = SweepSphereAgainstCapsule(start, end, 0.1f, general);
        REQUIRE(legacy.has_value() == current.has_value());
        if (legacy) CHECK(*current == doctest::Approx(*legacy));
    }
}

TEST_CASE("upright sweep against box returns physical face contact and supports high speed") {
    const VerticalCapsule c{{-2.0f, 0.0f, 0.5f}, 1.8f, 0.25f};
    const Aabb box{{0.0f, 0.0f, 0.0f}, {1.0f, 3.0f, 1.0f}};
    const auto hit = SweepVerticalCapsuleAgainstAabb(c, {1000.0f, 0.0f, 0.0f}, box);
    REQUIRE(hit);
    CHECK(hit->fraction == doctest::Approx(0.00175f));
    CHECK(hit->normal.x == doctest::Approx(-1.0f));
    CHECK(hit->normal.y == doctest::Approx(0.0f));
    CHECK(hit->position.x == doctest::Approx(0.0f));
    CHECK(hit->position.z == doctest::Approx(0.5f));
    CHECK(hit->penetrationDepth == 0.0f);
    CHECK_FALSE(SweepVerticalCapsuleAgainstAabb(c, {-2.0f, 0.0f, 0.0f}, box));
    CHECK_FALSE(SweepVerticalCapsuleAgainstAabb(c, {1.0f, 0.0f, 0.0f}, box));
}

TEST_CASE("box sweep respects rounded corners instead of expanded box corners") {
    const Aabb box{{0.0f, 0.0f, 0.0f}, {1.0f, 3.0f, 1.0f}};
    const VerticalCapsule nearCorner{{-0.2f, 0.0f, -0.2f}, 1.8f, 0.25f};
    CHECK_FALSE(OverlapVerticalCapsuleAabb(nearCorner, box));
    CHECK_FALSE(SweepVerticalCapsuleAgainstAabb(nearCorner, {0.0f, 0.1f, 0.0f}, box));

    const VerticalCapsule c{{-1.0f, 0.0f, -0.2f}, 1.8f, 0.25f};
    const auto hit = SweepVerticalCapsuleAgainstAabb(c, {2.0f, 0.0f, 0.0f}, box);
    REQUIRE(hit);
    CHECK(hit->fraction == doctest::Approx(0.425f));
    CHECK(hit->normal.x == doctest::Approx(-0.6f));
    CHECK(hit->normal.z == doctest::Approx(-0.8f));
    CHECK(hit->position.x == doctest::Approx(0.0f));
    CHECK(hit->position.z == doctest::Approx(0.0f));

    const auto diagonal = SweepVerticalCapsuleAgainstAabb({{-1,0,-1},1.8f,0.25f}, {2,0,2}, box);
    REQUIRE(diagonal);
    CHECK(diagonal->fraction == doctest::Approx((1.0f - 0.25f / std::sqrt(2.0f)) / 2.0f));
    CHECK(diagonal->normal.x == doctest::Approx(-1.0f / std::sqrt(2.0f)));
    CHECK(diagonal->normal.z == doctest::Approx(diagonal->normal.x));
}

TEST_CASE("box sweeps handle floor, ceiling and three dimensional rounded corners") {
    const Aabb floor{{-10.0f, -1.0f, -10.0f}, {10.0f, 0.0f, 10.0f}};
    const auto falling = SweepVerticalCapsuleAgainstAabb({{0,2,0},1.8f,0.25f}, {0,-4,0}, floor);
    REQUIRE(falling);
    CHECK(falling->fraction == doctest::Approx(0.5f));
    CHECK(falling->normal.y == doctest::Approx(1.0f));
    CHECK(falling->position.y == doctest::Approx(0.0f));
    const Aabb ceiling{{-10,3,-10},{10,4,10}};
    const auto rising = SweepVerticalCapsuleAgainstAabb({{0,0,0},1.8f,0.25f}, {0,2,0}, ceiling);
    REQUIRE(rising);
    CHECK(rising->fraction == doctest::Approx(0.6f));
    CHECK(rising->normal.y == doctest::Approx(-1.0f));
    CHECK(rising->position.y == doctest::Approx(3.0f));

    // A spherical capsule approaches a box vertex equally on all three axes.
    const Aabb box{{0,0,0},{1,1,1}};
    const auto vertex = SweepVerticalCapsuleAgainstAabb({{-1,-1.5f,-1},1.0f,0.5f}, {2,2,2}, box);
    REQUIRE(vertex);
    CHECK(vertex->fraction == doctest::Approx((1.0f - 0.5f / std::sqrt(3.0f)) / 2.0f));
    CHECK(vertex->normal.x == doctest::Approx(-1.0f / std::sqrt(3.0f)));
    CHECK(vertex->normal.y == doctest::Approx(vertex->normal.x));
    CHECK(vertex->normal.z == doctest::Approx(vertex->normal.x));
}

TEST_CASE("initial box penetration returns a usable minimum translation") {
    const Aabb box{{0,0,0},{1,3,1}};
    const auto edge = OverlapVerticalCapsuleAabb({{-0.1f,0,0.5f},1.8f,0.25f}, box);
    REQUIRE(edge);
    CHECK(edge->fraction == 0.0f);
    CHECK(edge->normal.x == -1.0f);
    CHECK(edge->penetrationDepth == doctest::Approx(0.15f));
    const auto inside = OverlapVerticalCapsuleAabb({{0.1f,0,0.5f},1.8f,0.25f}, box);
    REQUIRE(inside);
    CHECK(inside->normal.x == -1.0f);
    CHECK(inside->penetrationDepth == doctest::Approx(0.35f));
    CHECK(inside->position.x == 0.0f);
    const auto stationary = SweepVerticalCapsuleAgainstAabb({{0.1f,0,0.5f},1.8f,0.25f}, {}, box);
    REQUIRE(stationary);
    CHECK(stationary->penetrationDepth == doctest::Approx(inside->penetrationDepth));
}

TEST_CASE("initial touching allows sliding and separating but blocks entering") {
    const VerticalCapsule c{{-0.25f,0,0.5f},1.8f,0.25f};
    const Aabb box{{0,0,0},{1,3,1}};
    REQUIRE(OverlapVerticalCapsuleAabb(c, box));
    CHECK_FALSE(SweepVerticalCapsuleAgainstAabb(c, {-1,0,0}, box));
    CHECK_FALSE(SweepVerticalCapsuleAgainstAabb(c, {0,0,1}, box));
    const auto into = SweepVerticalCapsuleAgainstAabb(c, {1,0,0}, box);
    REQUIRE(into);
    CHECK(into->fraction == 0.0f);
    CHECK(into->penetrationDepth == 0.0f);

    const Aabb floor{{-10,-1,-10},{10,0,10}};
    CHECK_FALSE(SweepVerticalCapsuleAgainstAabb({{0,0,0},1.8f,0.25f}, {5,0,0}, floor));
}

TEST_CASE("capsule pair sweeps return combined radii and preserve height separation") {
    const VerticalCapsule target{{0,0,0},1.8f,0.3f};
    const auto hit = SweepVerticalCapsuleAgainstCapsule({{-2,0,0},1.6f,0.2f}, {4,0,0}, target);
    REQUIRE(hit);
    CHECK(hit->fraction == doctest::Approx(0.375f));
    CHECK(hit->normal.x == doctest::Approx(-1.0f));
    CHECK(hit->position.x == doctest::Approx(-0.3f));
    CHECK(hit->penetrationDepth == 0.0f);
    CHECK_FALSE(SweepVerticalCapsuleAgainstCapsule({{-2,2,0},1.6f,0.2f}, {4,0,0}, target));
    const auto falling = SweepVerticalCapsuleAgainstCapsule({{0,2.8f,0},1.6f,0.2f}, {0,-2,0}, target);
    REQUIRE(falling);
    CHECK(falling->fraction == doctest::Approx(0.5f));
    CHECK(falling->normal.y == doctest::Approx(1.0f));
    CHECK(falling->position.y == doctest::Approx(1.8f));
}

TEST_CASE("capsule pair overlap has finite normal even when center axes coincide") {
    const VerticalCapsule target{{0,0,0},1.8f,0.25f};
    const auto centered = OverlapVerticalCapsules(target, target);
    REQUIRE(centered);
    CHECK(centered->normal.x == 1.0f);
    CHECK(centered->penetrationDepth == doctest::Approx(0.5f));
    const auto overlap = OverlapVerticalCapsules({{-0.4f,0,0},1.8f,0.25f}, target);
    REQUIRE(overlap);
    CHECK(overlap->normal.x == -1.0f);
    CHECK(overlap->penetrationDepth == doctest::Approx(0.1f));
    const VerticalCapsule touching{{-0.5f,0,0},1.8f,0.25f};
    CHECK_FALSE(SweepVerticalCapsuleAgainstCapsule(touching, {-1,0,0}, target));
    CHECK_FALSE(SweepVerticalCapsuleAgainstCapsule(touching, {0,0,1}, target));
    const auto entering = SweepVerticalCapsuleAgainstCapsule(touching, {1,0,0}, target);
    REQUIRE(entering);
    CHECK(entering->fraction == 0.0f);
}

TEST_CASE("extended queries reject invalid geometry and nonfinite movement") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const Capsule valid{{0,0,0},{1,1,0},0.2f};
    CHECK_THROWS_AS(static_cast<void>(RaycastCapsule({}, {}, 1.0f, valid)), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(RaycastCapsule({}, {1,0,0}, -1.0f, valid)), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(SweepSphereAgainstCapsule({}, {nan,0,0}, 0.0f, valid)), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(SweepSphereAgainstCapsule({}, {}, 0.0f, Capsule{{},{},0})), std::invalid_argument);
    const VerticalCapsule upright{{},1.8f,0.25f};
    const Aabb box{{0,0,0},{1,1,1}};
    CHECK_THROWS_AS(static_cast<void>(SweepVerticalCapsuleAgainstAabb(upright, {nan,0,0}, box)), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(SweepVerticalCapsuleAgainstCapsule(upright, {}, {{},0.1f,0.25f})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(OverlapVerticalCapsuleAabb(upright, {{1,0,0},{0,1,1}})), std::invalid_argument);
}
