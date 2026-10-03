// Characterization of the object_fps_pvp presentation formulas that
// math-foundation batch B6b replaces with Engine::Math (frozen copies in
// PresentationLegacyMath.hpp; production-content checks live in
// PlayerPresentationTests.cpp).
//
// Bit-identical replacements are compared exactly. Four replacements drift and
// are held to derived bounds:
//   - the weapon muzzle and the third-person weapon position, which now use the
//     renderer's ComposeEulerXYZ matrix instead of step-by-step rotations;
//   - the weapon mount quaternion, now Math::Normalize in float instead of a
//     double reciprocal;
//   - the double horizontal length, now sqrt instead of hypot.

#include <doctest/doctest.h>

#include "CharacterizationSupport.hpp"
#include "PresentationLegacyMath.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/Vec3d.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Constants.hpp"
#include "engine/math/scalar/Scalar.hpp"
#include "render/RenderTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <ostream>

namespace {

namespace Math = Engine::Math;
using CharacterizationSupport::Opaque;
using CharacterizationSupport::Random;
using CharacterizationSupport::SameBits;
using CharacterizationSupport::UlpDistance;

[[nodiscard]] Math::Vec3 OpaqueVector(Random& random, const float scale) {
    return {Opaque(random.Any(scale)), Opaque(random.Any(scale)), Opaque(random.Any(scale))};
}

[[nodiscard]] bool SameBits(const Math::Vec3 a, const Math::Vec3 b) {
    return SameBits(a.x, b.x) && SameBits(a.y, b.y) && SameBits(a.z, b.z);
}

[[nodiscard]] float AbsoluteSum(const Math::Vec3 v) { return std::abs(v.x) + std::abs(v.y) + std::abs(v.z); }

// Unit roundoff of float.
constexpr float kRoundoff = 0x1p-24F;

} // namespace

TEST_CASE("characterization: presentation interpolation, angle and box helpers equal Math") {
    // PvpApplication.cpp:537 and :746 used this literal for the world camera.
    static_assert(Math::DegreesToRadians(60.0F) == 1.0471975512F);
    Random random{0x1D7E25A9U};
    std::size_t differing = 0;
    for (int i = 0; i < 100000; ++i) {
        const auto a = OpaqueVector(random, 64.0F);
        const auto b = OpaqueVector(random, 64.0F);
        const float fraction = Opaque(random.Unit());
        // SnapshotTimeline.hpp:115-117 and :120.
        const Math::Vec3 legacyPosition{a.x + (b.x - a.x) * fraction, a.y + (b.y - a.y) * fraction,
                                        a.z + (b.z - a.z) * fraction};
        if (!SameBits(legacyPosition, Math::Lerp(a, b, fraction))) ++differing;
        if (!SameBits(a.x + (b.x - a.x) * fraction, Math::Lerp(a.x, b.x, fraction))) ++differing;
        // SnapshotTimeline.hpp:118-119 and :175; PvpApplication.cpp:810-811.
        const float yawA = Opaque(i % 4 == 0 ? random.Any(8.0F) : random.Range(-4.0F, 4.0F));
        const float yawB = Opaque(random.Range(-4.0F, 4.0F));
        if (!SameBits(PresentationLegacy::LerpYaw(yawA, yawB, fraction),
                      Math::LerpRadiansShortest(yawA, yawB, fraction)))
            ++differing;
        if (!SameBits(std::remainder(yawA - yawB, 2 * std::numbers::pi_v<float>), Math::WrapRadians(yawA - yawB)))
            ++differing;
        if (!SameBits(PresentationLegacy::WrapMouseYaw(yawA), Math::WrapRadians(yawA))) ++differing;
        // WeaponPresentationDefinition.cpp:141-142 and :153.
        const float degrees = Opaque(random.Any(400.0F));
        if (!SameBits(PresentationLegacy::DegreesToRadians(degrees), Math::DegreesToRadians(degrees))) ++differing;
        // PvpApplication.cpp:557-558 (wall box), PlayerPresentation.cpp:548 and
        // WeaponViewModel.cpp:110-111 (negated anchors).
        const Math::Aabb box{a, b};
        const Math::Vec3 legacyCenter{(a.x + b.x) * .5F, (a.y + b.y) * .5F, (a.z + b.z) * .5F};
        if (!SameBits(legacyCenter, Math::Center(box))) ++differing;
        if (!SameBits(Math::Vec3{b.x - a.x, b.y - a.y, b.z - a.z}, box.maximum - box.minimum)) ++differing;
        if (!SameBits(Math::Vec3{-a.x, -a.y, -a.z}, -a)) ++differing;
        // PvpApplication.cpp:542-543 and :871, PlayerPresentation.cpp:129-130 and :289-290.
        if (!SameBits((std::min)(a.x, b.x), Math::Min(a.x, b.x))) ++differing;
        if (!SameBits((std::max)(a.y, b.y), Math::Max(a.y, b.y))) ++differing;
        // SnapshotTimeline.hpp:46-47, PlayerPresentation.cpp:23-25,
        // WeaponPresentationDefinition.cpp:19-21, CharacterPresentationDefinition.cpp:202-204.
        const bool legacyFinite = std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
        if (legacyFinite != Math::IsFinite(a)) ++differing;
    }
    CHECK(differing == 0);
}

TEST_CASE("characterization: presentation backward decision equals the Math dot product") {
    // PlayerPresentation.cpp:268: dx * sin(yaw) + dz * cos(yaw) < -1e-6. The
    // Math form adds the y term 0 * 0, so only the sign of an exact zero can
    // differ, which cannot change the decision.
    Random random{0x6B0F4C21U};
    std::size_t decisions = 0;
    std::size_t differingValues = 0;
    for (int i = 0; i < 100000; ++i) {
        const double dx = Opaque(static_cast<double>(random.Range(-0.2F, 0.2F)));
        const double dz = Opaque(static_cast<double>(i % 7 == 0 ? 0.0F : random.Range(-0.2F, 0.2F)));
        const float yaw = Opaque(random.Range(-4.0F, 4.0F));
        const double legacy = dx * std::sin(yaw) + dz * std::cos(yaw);
        const double math = Math::Dot(Math::Vec3d{dx, 0.0, dz}, Math::Vec3d{std::sin(yaw), 0.0, std::cos(yaw)});
        if ((legacy < -0.000001) != (math < -0.000001)) ++decisions;
        if (legacy != math) ++differingValues; // -0 == +0
    }
    CHECK(decisions == 0);
    CHECK(differingValues == 0);
}

// The horizontal length of a double XZ difference changed from std::hypot(dx,
// dz) to Math::Length({dx, 0, dz}) = sqrt(dx*dx + 0*0 + dz*dz). The squares,
// the sum and the sqrt keep the result within 2u relative (u = 2^-53); hypot
// is assumed accurate to 1 ulp (2u relative) on the supported libms. The two
// therefore differ by less than 4u relative, under 4 ulp.
constexpr std::uint64_t kHorizontalLengthDriftUlpBound = 4;

TEST_CASE("characterization: presentation horizontal length drifts from Math Length within a derived bound") {
    Random random{0x58C3A70DU};
    std::uint64_t maximumUlp = 0;
    std::size_t differing = 0;
    constexpr int kCount = 200000;
    for (int i = 0; i < kCount; ++i) {
        // Differences of float positions, as both call sites form them; the
        // exponents of the two positions vary independently.
        const float magnitudeA = std::pow(10.0F, random.Range(-4.0F, 2.0F));
        const float magnitudeB = std::pow(10.0F, random.Range(-4.0F, 2.0F));
        const float ax = Opaque(random.Range(-1.0F, 1.0F) * magnitudeA);
        const float az = Opaque(random.Range(-1.0F, 1.0F) * magnitudeA);
        const float bx = Opaque(random.Range(-1.0F, 1.0F) * magnitudeB);
        const float bz = Opaque(random.Range(-1.0F, 1.0F) * magnitudeB);
        const double dx = static_cast<double>(bx) - ax;
        const double dz = static_cast<double>(bz) - az;
        const double legacy = PresentationLegacy::HorizontalLength(dx, dz);
        const double math = Math::Length(Math::Vec3d{dx, 0.0, dz});
        const auto ulp = UlpDistance(legacy, math);
        maximumUlp = (std::max)(maximumUlp, ulp);
        if (ulp != 0) ++differing;
    }
    MESSAGE("horizontal length: max " << maximumUlp << " ulp, " << differing << " of " << kCount << " differ");
    CHECK(maximumUlp <= kHorizontalLengthDriftUlpBound);
}

// The muzzle and the third-person weapon position now apply the renderer's
// world matrix, ComposeEulerXYZ(translation, rotation, scale), to the anchored
// point instead of scaling and rotating it step by step. Both evaluate the
// same exact value with different rounding. Every intermediate is bounded by
// M = |translation| + max(scale) * |point - anchor|_1 (rotation entries are at
// most 1 in magnitude), and each path rounds fewer than 16 times along any
// component, so the results differ by less than 32 u M (u = 2^-24).
constexpr float kWorldTransformDriftBound = 32.0F;

TEST_CASE("characterization: presentation world transforms drift from ComposeEulerXYZ within a derived bound") {
    Random random{0x0C61B3E7U};
    float maximumRatio = 0.0F;
    std::size_t differing = 0;
    std::size_t identicalWhenOnlyYaw = 0;
    std::size_t onlyYawSamples = 0;
    constexpr int kCount = 200000;
    for (int i = 0; i < kCount; ++i) {
        const Math::Vec3 point{Opaque(random.Range(-0.4F, 0.4F)), Opaque(random.Range(-0.4F, 0.4F)),
                               Opaque(random.Range(0.1F, 0.8F))};
        const Math::Vec3 anchor{Opaque(random.Range(-0.2F, 0.2F)), Opaque(random.Range(-0.3F, 0.3F)),
                                Opaque(random.Range(-0.2F, 0.2F))};
        Engine::Render::Transform3D transform;
        const bool onlyYaw = i % 2 == 0;
        if (onlyYaw) {
            // The muzzle at load: rotation (0, yaw, 0) and scale 1.
            transform.translation = {Opaque(random.Range(-0.3F, 0.3F)), Opaque(random.Range(-0.3F, 0.3F)),
                                     Opaque(random.Range(0.2F, 0.8F))};
            transform.rotationRadians = {0.0F, Opaque(random.Range(-4.0F, 4.0F)), 0.0F};
        } else {
            transform.translation = {Opaque(random.Range(-60.0F, 60.0F)), Opaque(random.Range(-2.0F, 6.0F)),
                                     Opaque(random.Range(-60.0F, 60.0F))};
            transform.rotationRadians = {Opaque(random.Range(-0.1F, 0.1F)), Opaque(random.Range(-4.0F, 4.0F)),
                                         Opaque(random.Range(-0.1F, 0.1F))};
            const float scale = Opaque(random.Range(0.05F, 2.0F));
            transform.scale = {scale, scale, scale};
        }
        const auto legacy = PresentationLegacy::MuzzleViewCameraPosition(point, anchor, transform);
        const auto math = Math::TransformPoint(
            Math::ComposeEulerXYZ(transform.translation, transform.rotationRadians, transform.scale), point - anchor);
        if (onlyYaw) {
            // Equal values; only the sign of an exact zero may differ.
            ++onlyYawSamples;
            if (legacy.x == math.x && legacy.y == math.y && legacy.z == math.z) ++identicalWhenOnlyYaw;
        }
        if (!SameBits(legacy, math)) ++differing;
        const float spread = transform.scale.x * AbsoluteSum(point - anchor);
        const float limits[3]{std::abs(transform.translation.x) + spread, std::abs(transform.translation.y) + spread,
                              std::abs(transform.translation.z) + spread};
        const float errors[3]{std::abs(legacy.x - math.x), std::abs(legacy.y - math.y), std::abs(legacy.z - math.z)};
        for (int axis = 0; axis < 3; ++axis) {
            maximumRatio = (std::max)(maximumRatio, errors[axis] / (kRoundoff * limits[axis]));
        }
        // The third-person weapon: rotation (0, yaw, 0), uniform scale.
        if (!onlyYaw) {
            const float yaw = transform.rotationRadians.y;
            const auto legacyWorld = PresentationLegacy::WeaponWorldPosition(
                point, anchor, transform.scale.x, transform.translation, yaw);
            const auto mathWorld = Math::TransformPoint(
                Math::ComposeEulerXYZ(transform.translation, {0.0F, yaw, 0.0F}, transform.scale), point - anchor);
            const float worldErrors[3]{std::abs(legacyWorld.x - mathWorld.x), std::abs(legacyWorld.y - mathWorld.y),
                                       std::abs(legacyWorld.z - mathWorld.z)};
            for (int axis = 0; axis < 3; ++axis) {
                maximumRatio = (std::max)(maximumRatio, worldErrors[axis] / (kRoundoff * limits[axis]));
            }
        }
    }
    MESSAGE("world transforms: max error " << maximumRatio << " u M, " << differing << " of " << kCount
                                           << " muzzle samples differ, " << identicalWhenOnlyYaw << " of "
                                           << onlyYawSamples << " yaw-only unit-scale samples identical");
    CHECK(maximumRatio <= kWorldTransformDriftBound);
    // With rotation only about Y and scale 1 every matrix entry is exact, so the
    // muzzle at load time (production content) does not drift at all.
    CHECK(identicalWhenOnlyYaw == onlyYawSamples);
}

// The weapon mount rotation changed from multiplying by a float-rounded double
// reciprocal (within 1 ulp of the exact result) to Math::Normalize, whose float
// sum of squares, sqrt and division stay within 3 ulp; the two differ by at
// most 4 ulp per component.
constexpr std::int64_t kMountNormalizeDriftUlpBound = 4;

TEST_CASE("characterization: presentation mount rotation drifts from Math Normalize within a derived bound") {
    Random random{0x7F21D9C5U};
    std::int64_t maximumUlp = 0;
    std::size_t acceptanceFlips = 0;
    constexpr int kCount = 200000;
    for (int i = 0; i < kCount; ++i) {
        const float magnitude = std::pow(10.0F, random.Range(-5.0F, 9.0F));
        const Math::Quaternion q{Opaque(random.Range(-1.0F, 1.0F) * magnitude), Opaque(random.Range(-1.0F, 1.0F) * magnitude),
                                 Opaque(random.Range(-1.0F, 1.0F) * magnitude), Opaque(random.Range(-1.0F, 1.0F) * magnitude)};
        const float lengthSquared = Math::LengthSquared(q);
        const bool accepted = std::isfinite(lengthSquared) && !(lengthSquared < 1e-12F);
        const double legacyLengthSquared = static_cast<double>(q.x) * q.x + static_cast<double>(q.y) * q.y +
            static_cast<double>(q.z) * q.z + static_cast<double>(q.w) * q.w;
        if (legacyLengthSquared >= 1e-10 && legacyLengthSquared <= 1e18 &&
            accepted != PresentationLegacy::AcceptsMountRotation(q))
            ++acceptanceFlips;
        if (!accepted) continue;
        const auto legacy = PresentationLegacy::NormalizeMountRotation(q);
        const auto math = Math::Normalize(q);
        maximumUlp = (std::max)({maximumUlp, UlpDistance(legacy.x, math.x), UlpDistance(legacy.y, math.y),
                                 UlpDistance(legacy.z, math.z), UlpDistance(legacy.w, math.w)});
    }
    MESSAGE("mount rotation: max " << maximumUlp << " ulp, " << acceptanceFlips << " acceptance flips");
    CHECK(maximumUlp <= kMountNormalizeDriftUlpBound);
    // Inside |q|^2 in [1e-10, 1e18] the float and double validations agree.
    CHECK(acceptanceFlips == 0);
}
