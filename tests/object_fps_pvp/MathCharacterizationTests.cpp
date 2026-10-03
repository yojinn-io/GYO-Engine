// Characterization of the object_fps_pvp simulation helpers that
// math-foundation batch B6a replaces with Engine::Math.
//
// Each Legacy function is frozen from master efe4a30 with the same expression
// shape, operand order and types (file:line in its comment). The replacements
// must be bit-identical, except two measured drifts with derived bounds:
//   - ComputePlanarInput's reciprocal-multiply normalization (authority and
//     prediction movement), and
//   - CharacterCollision's hypot horizontal length (only on the
//     constrainToFloor path, which compiled PvP does not use).
//
// All inputs reach the code through volatile reads, so the compiler cannot
// constant-fold either side with a rounding that differs from run time.

#include <doctest/doctest.h>

#include "CharacterizationSupport.hpp"
#include "RetroFPS/Gameplay/Player/PlanarMovement.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "engine/math/geometry/Intersection.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/Vec3d.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Constants.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <ostream>
#include <vector>

namespace {

namespace Math = Engine::Math;
using CharacterizationSupport::Opaque;
using CharacterizationSupport::Random;
using CharacterizationSupport::SameBits;
using CharacterizationSupport::UlpDistance;

namespace Legacy {

struct Float2 {
    float x = 0.0f;
    float z = 0.0f;
};

struct Float3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Movement.hpp:25.
constexpr float MovementMaximumPitch = 89.0F * std::numbers::pi_v<float> / 180.0F;
// Movement.cpp:17.
constexpr float PitchBound = std::numbers::pi_v<float> / 2;

// Movement.cpp:25, ShotQuery.cpp:21, PvpMatch.cpp:34 and :237.
[[nodiscard]] float WrapYaw(const float yaw) { return std::remainder(yaw, 2 * std::numbers::pi_v<float>); }

// Movement.cpp:26, ShotQuery.cpp:22.
[[nodiscard]] float ClampPitch(const float pitch) {
    return std::clamp(pitch, -MovementMaximumPitch, MovementMaximumPitch);
}

// LocalPlayerPrediction.cpp:13-17.
[[nodiscard]] Float3 Interpolate(Float3 from, Float3 to, float alpha) {
    return {from.x + (to.x - from.x) * alpha,
            from.y + (to.y - from.y) * alpha,
            from.z + (to.z - from.z) * alpha};
}

// LocalPlayerPrediction.cpp:18-20.
[[nodiscard]] Float3 Difference(Float3 lhs, Float3 rhs) { return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z}; }

// LocalPlayerPrediction.cpp:30-32.
[[nodiscard]] float Length(Float3 value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

// LocalPlayerPrediction.cpp:33-35 and Arena.cpp:11-13.
[[nodiscard]] bool Finite(Float3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

// ShotQuery.cpp:26-28.
[[nodiscard]] Float3 NormalizeDirection(Float3 direction) {
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
        direction.z * direction.z);
    return {direction.x / length, direction.y / length, direction.z / length};
}

// ShotQuery.cpp:32-37, for an origin above the floor (origin.y > 0).
[[nodiscard]] std::optional<float> FloorDistance(Float3 origin, Float3 direction, float range) {
    if (direction.y < 0) {
        const float distance = -origin.y / direction.y;
        if (distance <= range) return distance;
    }
    return std::nullopt;
}

// PvpMatch.cpp:197-200, one blocker.
[[nodiscard]] double SpawnDistanceSquared(Float3 p, Float3 feet) {
    const double dx = p.x - feet.x;
    const double dy = p.y - feet.y;
    const double dz = p.z - feet.z;
    return dx * dx + dy * dy + dz * dz;
}

// CharacterCollision.cpp:101-103.
[[nodiscard]] Float3 Depenetrate(Float3 feet, Float3 normal, float depth) {
    feet.x += normal.x * (depth + 0.0001F);
    feet.y += normal.y * (depth + 0.0001F);
    feet.z += normal.z * (depth + 0.0001F);
    return feet;
}

struct SweepStep {
    Float3 feet;
    Float3 displacement;
    float fraction;
};

// CharacterCollision.cpp:124-140 for a sweep that hit at nearestFraction.
[[nodiscard]] SweepStep Slide(Float3 feet, Float3 d, float nearestFraction, Float3 n) {
    const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    const float fraction = (std::max)(0.0F, nearestFraction - 0.0001F / length);
    feet.x += d.x * fraction;
    feet.y += d.y * fraction;
    feet.z += d.z * fraction;
    Float3 displacement{d.x * (1 - fraction), d.y * (1 - fraction), d.z * (1 - fraction)};
    const float into =
        (std::min)(0.0F, displacement.x * n.x + displacement.y * n.y + displacement.z * n.z);
    displacement.x -= n.x * into;
    displacement.y -= n.y * into;
    displacement.z -= n.z * into;
    return {feet, displacement, fraction};
}

// CharacterCollision.cpp:17 and :111.
[[nodiscard]] float HorizontalLength(Float3 normal) { return std::hypot(normal.x, normal.z); }

// PlanarMovement.cpp:8-32.
[[nodiscard]] Float2 ComputePlanarInput(
    const float forwardAxis,
    const float rightAxis,
    const float yawRadians) noexcept {
    const float clampedForward = std::clamp(forwardAxis, -1.0f, 1.0f);
    const float clampedRight = std::clamp(rightAxis, -1.0f, 1.0f);
    const float sinYaw = std::sin(yawRadians);
    const float cosYaw = std::cos(yawRadians);

    Float2 movement{
        sinYaw * clampedForward + cosYaw * clampedRight,
        cosYaw * clampedForward - sinYaw * clampedRight,
    };

    const float lengthSquared = movement.x * movement.x + movement.z * movement.z;
    if (lengthSquared > 1.0f) {
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        movement.x *= inverseLength;
        movement.z *= inverseLength;
    }

    return movement;
}

} // namespace Legacy

[[nodiscard]] Math::Vec3 ToMath(const Legacy::Float3 v) { return {v.x, v.y, v.z}; }

[[nodiscard]] Legacy::Float3 Opaque(const Legacy::Float3 value) {
    return {Opaque(value.x), Opaque(value.y), Opaque(value.z)};
}

[[nodiscard]] bool SameBits(const Legacy::Float3 a, const Math::Vec3 b) {
    return SameBits(a.x, b.x) && SameBits(a.y, b.y) && SameBits(a.z, b.z);
}

[[nodiscard]] Legacy::Float3 AnyVector(Random& random, const float scale) {
    return {random.Any(scale), random.Any(scale), random.Any(scale)};
}

[[nodiscard]] std::vector<float> YawSamples() {
    constexpr float pi = std::numbers::pi_v<float>;
    std::vector<float> values{0.0F, -0.0F, pi, -pi, std::nextafter(pi, 0.0F), std::nextafter(pi, 4.0F),
                              2 * pi, -2 * pi, 3 * pi, 7.5F * pi, 1.0e6F, -1.0e6F,
                              std::nextafter(1.0e6F, 0.0F), 1.0e-40F,
                              std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()};
    Random random{0x7A3C11D2U};
    for (int i = 0; i < 100000; ++i) values.push_back(random.Range(-1.0e6F, 1.0e6F));
    for (int i = 0; i < 100000; ++i) values.push_back(random.Range(-8 * pi, 8 * pi));
    return values;
}

} // namespace

TEST_CASE("characterization: pvp angle constants equal Math constants") {
    static_assert(Math::DegreesToRadians(89.0F) == Legacy::MovementMaximumPitch);
    static_assert(fps::pvp::MovementMaximumPitch == Legacy::MovementMaximumPitch);
    static_assert(Math::HalfPi == Legacy::PitchBound);
    CHECK(SameBits(Math::DegreesToRadians(Opaque(89.0F)), Legacy::MovementMaximumPitch));
}

TEST_CASE("characterization: pvp yaw wrap and pitch clamp equal Math WrapRadians and Clamp") {
    std::size_t differing = 0;
    for (const float sample : YawSamples()) {
        const float input = Opaque(sample);
        if (!SameBits(Legacy::WrapYaw(input), Math::WrapRadians(input))) ++differing;
        if (!SameBits(Legacy::ClampPitch(input),
                      Math::Clamp(input, -fps::pvp::MovementMaximumPitch, fps::pvp::MovementMaximumPitch)))
            ++differing;
    }
    CHECK(differing == 0);
}

TEST_CASE("characterization: pvp prediction vector helpers equal Math") {
    Random random{0x51F0A6E3U};
    std::size_t differing = 0;
    for (int i = 0; i < 100000; ++i) {
        const auto a = Opaque(AnyVector(random, 64.0F));
        const auto b = Opaque(AnyVector(random, 64.0F));
        const float alpha = Opaque(i % 8 == 0 ? random.Any(2.0F) : random.Unit());
        const float scale = Opaque(random.Unit());
        if (!SameBits(Legacy::Interpolate(a, b, alpha), Math::Lerp(ToMath(a), ToMath(b), alpha))) ++differing;
        if (!SameBits(Legacy::Difference(a, b), ToMath(a) - ToMath(b))) ++differing;
        if (!SameBits(Legacy::Length(a), Math::Length(ToMath(a)))) ++differing;
        if (Legacy::Finite(a) != Math::IsFinite(ToMath(a))) ++differing;
        // LocalPlayerPrediction.cpp:210-212, :217-219, :341 and :354.
        const Legacy::Float3 sum{a.x + b.x, a.y + b.y, a.z + b.z};
        if (!SameBits(sum, ToMath(a) + ToMath(b))) ++differing;
        const Legacy::Float3 scaled{a.x * scale, a.y * scale, a.z * scale};
        if (!SameBits(scaled, ToMath(a) * scale)) ++differing;
        // LocalPlayerPrediction.cpp:361, CharacterCollision.cpp:142.
        if (!SameBits((std::max)(0.0F, a.y), Math::Max(0.0F, a.y))) ++differing;
    }
    CHECK(differing == 0);
}

TEST_CASE("characterization: pvp shot direction and floor distance equal Math Normalize and Intersect") {
    Random random{0x2C9B47F1U};
    std::size_t differing = 0;
    std::size_t floorHits = 0;
    for (int i = 0; i < 100000; ++i) {
        // Directions as QueryShot builds them, from this platform's sin/cos.
        const float yaw = Opaque(random.Range(-4.0F, 4.0F));
        const float pitch = Opaque(random.Range(-fps::pvp::MovementMaximumPitch, fps::pvp::MovementMaximumPitch));
        const float cosinePitch = std::cos(pitch);
        const Legacy::Float3 raw{std::sin(yaw) * cosinePitch, -std::sin(pitch), std::cos(yaw) * cosinePitch};
        const auto legacy = Legacy::NormalizeDirection(raw);
        const auto math = Math::Normalize(ToMath(raw));
        if (!SameBits(legacy, math)) ++differing;

        const Legacy::Float3 origin = Opaque(Legacy::Float3{
            random.Range(-40.0F, 40.0F), random.Range(1.0e-3F, 8.0F), random.Range(-40.0F, 40.0F)});
        const float range = Opaque(random.Range(0.0F, 120.0F));
        const auto legacyFloor = Legacy::FloorDistance(origin, legacy, range);
        auto mathFloor = Math::Intersect(Math::Ray{ToMath(origin), math}, Math::Plane{});
        if (mathFloor && !(*mathFloor <= range)) mathFloor.reset();
        if (legacyFloor.has_value() != mathFloor.has_value() ||
            (legacyFloor && !SameBits(*legacyFloor, *mathFloor)))
            ++differing;
        if (legacyFloor) ++floorHits;
    }
    // A general vector (not only unit-length directions) normalizes identically.
    for (int i = 0; i < 100000; ++i) {
        const auto value = Opaque(AnyVector(random, 1.0e3F));
        if (!SameBits(Legacy::NormalizeDirection(value), Math::Normalize(ToMath(value)))) ++differing;
    }
    CHECK(floorHits > 10000);
    CHECK(differing == 0);
}

TEST_CASE("characterization: pvp spawn distance equals Math LengthSquared of the float difference") {
    Random random{0x6D2E90B5U};
    std::size_t differing = 0;
    for (int i = 0; i < 100000; ++i) {
        const auto p = Opaque(AnyVector(random, 64.0F));
        const auto feet = Opaque(AnyVector(random, 64.0F));
        const double math = Math::LengthSquared(Math::ToVec3d(ToMath(p) - ToMath(feet)));
        if (!SameBits(Legacy::SpawnDistanceSquared(p, feet), math)) ++differing;
    }
    CHECK(differing == 0);
}

TEST_CASE("characterization: pvp character sweep and slide expressions equal Math operators") {
    Random random{0x0F4B83A9U};
    std::size_t differing = 0;
    for (int i = 0; i < 100000; ++i) {
        const auto feet = Opaque(AnyVector(random, 32.0F));
        const auto normal = Opaque(AnyVector(random, 1.0F));
        const float depth = Opaque(random.Any(0.5F));
        const Math::Vec3 pushed = ToMath(feet) + ToMath(normal) * (depth + 0.0001F);
        if (!SameBits(Legacy::Depenetrate(feet, normal, depth), pushed)) ++differing;

        const auto d = Opaque(AnyVector(random, 0.2F));
        const float nearestFraction = Opaque(random.Unit());
        const auto legacy = Legacy::Slide(feet, d, nearestFraction, normal);
        const Math::Vec3 md = ToMath(d);
        const float length = Math::Length(md);
        const float fraction = Math::Max(0.0F, nearestFraction - 0.0001F / length);
        const Math::Vec3 advanced = ToMath(feet) + md * fraction;
        Math::Vec3 displacement = md * (1 - fraction);
        const float into = Math::Min(0.0F, Math::Dot(displacement, ToMath(normal)));
        displacement = displacement - ToMath(normal) * into;
        if (!SameBits(legacy.fraction, fraction) || !SameBits(legacy.feet, advanced) ||
            !SameBits(legacy.displacement, displacement))
            ++differing;
        // CharacterCollision.cpp:33.
        if (!SameBits((std::max)(normal.x, depth), Math::Max(normal.x, depth))) ++differing;
    }
    CHECK(differing == 0);
}

// ComputePlanarInput's normalization changed from x * (1 / sqrt(s)) to
// x / sqrt(s) (Math::Normalize; s is unchanged). x / sqrt(s) is correctly
// rounded; the legacy product carries the reciprocal's error (< 1 ulp of the
// result) plus its own rounding (0.5 ulp), so the two differ by less than
// 2 ulp: at most 1 ulp within a binade, 2 across a binade boundary. Inputs
// with s <= 1 are not normalized and must stay identical.
constexpr std::int64_t kPlanarNormalizeDriftUlpBound = 2;

TEST_CASE("characterization: pvp planar input normalization drifts from Math Normalize within a derived bound") {
    Random random{0x3B6A1C07U};
    std::int64_t maximumUlp = 0;
    std::size_t differing = 0;
    std::size_t normalized = 0;
    std::size_t unnormalizedDiffering = 0;
    const auto yaws = YawSamples();
    constexpr std::array<float, 9> axes{-1.0F, -0.70710677F, -0.5F, -0.0F, 0.0F, 0.5F, 0.70710677F, 0.99999994F, 1.0F};
    for (std::size_t i = 0; i < yaws.size(); ++i) {
        const float yaw = Opaque(std::isfinite(yaws[i]) ? yaws[i] : 0.0F);
        const float forward = Opaque(i % 2 == 0 ? axes[i % axes.size()] : random.Range(-1.25F, 1.25F));
        const float right = Opaque(i % 3 == 0 ? axes[(i / 3) % axes.size()] : random.Range(-1.25F, 1.25F));
        const auto legacy = Legacy::ComputePlanarInput(forward, right, yaw);
        const auto current = fps::ComputePlanarInput(forward, right, yaw);
        const float sinYaw = std::sin(yaw);
        const float cosYaw = std::cos(yaw);
        const float clampedForward = std::clamp(forward, -1.0F, 1.0F);
        const float clampedRight = std::clamp(right, -1.0F, 1.0F);
        const float x = sinYaw * clampedForward + cosYaw * clampedRight;
        const float z = cosYaw * clampedForward - sinYaw * clampedRight;
        const bool wasNormalized = x * x + z * z > 1.0F;
        const std::int64_t ulp = (std::max)(UlpDistance(legacy.x, current.x), UlpDistance(legacy.z, current.z));
        if (wasNormalized) {
            ++normalized;
            maximumUlp = (std::max)(maximumUlp, ulp);
            if (ulp != 0) ++differing;
        } else if (ulp != 0) {
            ++unnormalizedDiffering;
        }
    }
    MESSAGE("planar normalization: max " << maximumUlp << " ulp, " << differing << " of " << normalized
                                         << " normalized inputs differ");
    CHECK(normalized > 20000);
    CHECK(unnormalizedDiffering == 0);
    CHECK(maximumUlp <= kPlanarNormalizeDriftUlpBound);
}

// CharacterCollision's horizontal length changed from hypotf(x, z) to
// Math::Length({x, 0, z}) = sqrt(x*x + 0*0 + z*z). The float sum of squares
// and the sqrt keep the result within 2u relative (u = 2^-24); hypotf is
// assumed accurate to 1 ulp (2u relative) on the supported libms. The lengths
// therefore differ by < 4u relative, under 4 ulp; each normalized component
// adds at most 0.5 ulp of rounding on each side.
constexpr std::int64_t kHorizontalLengthDriftUlpBound = 4;
constexpr std::int64_t kHorizontalDirectionDriftUlpBound = kHorizontalLengthDriftUlpBound + 1;

TEST_CASE("characterization: pvp horizontal contact normal drifts from Math Length within a derived bound") {
    Random random{0x48E5D21FU};
    std::int64_t lengthUlp = 0;
    std::int64_t directionUlp = 0;
    std::size_t gateFlips = 0;
    std::size_t gateFlipsAwayFromThreshold = 0;
    for (int i = 0; i < 200000; ++i) {
        // Contact normals are unit vectors. Their horizontal part spans
        // [1e-8, 1] log-uniformly, so the 1e-6 gate is crossed.
        const float horizontal = std::pow(Opaque(10.0F), random.Range(-8.0F, 0.0F));
        const float angle = random.Range(-4.0F, 4.0F);
        const Legacy::Float3 normal = Opaque(Legacy::Float3{std::sin(angle) * horizontal,
                                                            std::sqrt(1.0F - horizontal * horizontal),
                                                            std::cos(angle) * horizontal});
        const float legacy = Legacy::HorizontalLength(normal);
        const float math = Math::Length(Math::Vec3{normal.x, 0, normal.z});
        lengthUlp = (std::max)(lengthUlp, UlpDistance(legacy, math));
        const bool legacyGate = legacy > 0.000001F;
        const bool mathGate = math > 0.000001F;
        if (legacyGate != mathGate) {
            ++gateFlips;
            if (UlpDistance(legacy, 0.000001F) > kHorizontalLengthDriftUlpBound) ++gateFlipsAwayFromThreshold;
            continue;
        }
        if (!legacyGate) continue;
        directionUlp = (std::max)({directionUlp, UlpDistance(normal.x / legacy, normal.x / math),
                                   UlpDistance(normal.z / legacy, normal.z / math)});
    }
    MESSAGE("horizontal normal: length max " << lengthUlp << " ulp, direction max " << directionUlp
                                             << " ulp, " << gateFlips << " gate flips at 1e-6");
    CHECK(lengthUlp <= kHorizontalLengthDriftUlpBound);
    CHECK(directionUlp <= kHorizontalDirectionDriftUlpBound);
    CHECK(gateFlipsAwayFromThreshold == 0);
}
