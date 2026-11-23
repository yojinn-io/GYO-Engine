// The planar movement input's contract (PlanarMovement.hpp): the direction is
// +Z at yaw 0 and yaw turns +Z toward +X, it is never longer than 1, the pitch
// never changes it, and the displacement is direction x speed x time. master
// checked this unit only through its math characterization tests, which are
// not carried (porting inventory, section 8).
#include <doctest/doctest.h>

#include "object_fps_pvp/sim/PlanarMovement.hpp"

#include <cmath>
#include <numbers>

namespace {
using namespace object_fps_pvp::sim;

constexpr float HalfPi = std::numbers::pi_v<float> / 2;

float Length(const GroundPoint p) { return std::sqrt(p.x * p.x + p.z * p.z); }
} // namespace

TEST_CASE("PvP planar input faces +Z at yaw 0 and turns +Z toward +X with yaw") {
    const auto forward = ComputePlanarInput(1, 0, 0);
    CHECK(forward.x == doctest::Approx(0).epsilon(1e-6));
    CHECK(forward.z == doctest::Approx(1));
    const auto right = ComputePlanarInput(0, 1, 0);
    CHECK(right.x == doctest::Approx(1));
    CHECK(right.z == doctest::Approx(0).epsilon(1e-6));
    const auto turned = ComputePlanarInput(1, 0, HalfPi);
    CHECK(turned.x == doctest::Approx(1));
    CHECK(std::abs(turned.z) < 1e-6F);
    const auto backward = ComputePlanarInput(-1, 0, 0);
    CHECK(backward.z == doctest::Approx(-1));
}

TEST_CASE("PvP planar input is never longer than one and keeps smaller inputs") {
    const auto diagonal = ComputePlanarInput(1, 1, 0.3F);
    CHECK(Length(diagonal) == doctest::Approx(1));
    CHECK(diagonal.x > 0);
    const auto clamped = ComputePlanarInput(5, -7, 1.1F);
    CHECK(Length(clamped) == doctest::Approx(1));
    const auto half = ComputePlanarInput(0.5F, 0, 0);
    CHECK(Length(half) == doctest::Approx(0.5));
    const auto none = ComputePlanarInput(0, 0, 2.0F);
    CHECK(none.x == 0);
    CHECK(none.z == 0);
}

TEST_CASE("PvP planar input ignores the pitch") {
    for (const float pitch : {-1.5F, -0.4F, 0.0F, 0.9F, 1.5F}) {
        CAPTURE(pitch);
        const auto pitched = ComputePlanarInput(0.7F, -0.3F, 0.8F, pitch);
        const auto level = ComputePlanarInput(0.7F, -0.3F, 0.8F);
        CHECK(pitched.x == level.x);
        CHECK(pitched.z == level.z);
    }
}

TEST_CASE("PvP planar displacement is the direction times speed and time") {
    const auto direction = ComputePlanarInput(1, 1, 0.25F);
    const auto step = ComputePlanarDisplacement(1, 1, 0.25F, 3.0F, 0.5F);
    CHECK(step.x == doctest::Approx(direction.x * 1.5F));
    CHECK(step.z == doctest::Approx(direction.z * 1.5F));
    const auto still = ComputePlanarDisplacement(1, 0, 0, 0.0F, 1.0F);
    CHECK(still.x == 0);
    CHECK(still.z == 0);
}
