#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "engine/collision/Collision.hpp"
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
