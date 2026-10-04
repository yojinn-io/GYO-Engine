#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Capsule.hpp"
#include "engine/math/geometry/Intersection.hpp"
#include "engine/math/geometry/Plane.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/geometry/Rect.hpp"
#include "engine/math/geometry/Segment.hpp"
#include "engine/math/geometry/Sphere.hpp"
#include "engine/math/geometry/Triangle.hpp"
#include "engine/math/linear/Matrix3.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/Vec3d.hpp"
#include "engine/math/linear/Vec4.hpp"
#include "engine/math/linear/VecInt.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/ColorSpace.hpp"
#include "engine/math/scalar/Constants.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include "AssertTestSupport.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <type_traits>
#include <vector>

// Spec-derived tests for GYO::Math. Expected values come from the documented
// contract (docs/architecture/math.md, header comments) and hand calculation,
// never from the implementation's own expressions.

using namespace Engine::Math;

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
const float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kDenormMin = std::numeric_limits<float>::denorm_min();
constexpr float kFloatMin = (std::numeric_limits<float>::min)();

// Deterministic LCG (Numerical Recipes constants); no time-based seeds.
class Lcg final {
public:
    explicit Lcg(const std::uint32_t seed) : state_(seed) {}

    std::uint32_t Next() {
        state_ = state_ * 1664525U + 1013904223U;
        return state_;
    }

    // Uniform in [lo, hi].
    float Range(const float lo, const float hi) {
        const float unit = static_cast<float>(Next() >> 8U) / 16777216.0F;
        return lo + (hi - lo) * unit;
    }

    Vec3 RangeVec3(const float lo, const float hi) { return {Range(lo, hi), Range(lo, hi), Range(lo, hi)}; }

private:
    std::uint32_t state_;
};

// Each draw is its own statement so the sequence does not depend on the
// compiler's argument evaluation order.
Quaternion RandomRotation(Lcg& random, const Vec3 bias) {
    const Vec3 axis = Normalize(random.RangeVec3(-1.0F, 1.0F) + bias);
    const float angle = random.Range(-Pi, Pi);
    return MakeQuaternionFromAxisAngle(axis, angle);
}

Matrix4 RandomEulerTransform(Lcg& random) {
    const Vec3 translation = random.RangeVec3(-5.0F, 5.0F);
    const Vec3 rotation = random.RangeVec3(-Pi, Pi);
    const Vec3 scale = random.RangeVec3(0.5F, 2.0F);
    return ComposeEulerXYZ(translation, rotation, scale);
}

// Builds a column-major Matrix4 from rows written in reading order.
Matrix4 FromRows(const std::array<std::array<float, 4>, 4>& rows) {
    Matrix4 result;
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            result.values[column * 4 + row] = rows[row][column];
        }
    }
    return result;
}

Matrix3 FromRows3(const std::array<std::array<float, 3>, 3>& rows) {
    Matrix3 result;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            result.values[column * 3 + row] = rows[row][column];
        }
    }
    return result;
}

// Full homogeneous product M * v from the column-major definition
// (out[row] = sum over column of values[column * 4 + row] * v[column]).
Vec4 Apply(const Matrix4& matrix, const Vec4 v) {
    const float in[4]{v.x, v.y, v.z, v.w};
    float out[4]{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            out[row] += matrix.values[column * 4 + row] * in[column];
        }
    }
    return {out[0], out[1], out[2], out[3]};
}

// Hand-derived rotations from the documented conventions. A proper rotation
// is fixed by the image of two basis vectors:
//   X: (0,0,1) -> (0,-s,c) and (0,1,0) -> (0,c,s)
//   Y: (0,0,1) -> (s,0,c)  and (1,0,0) -> (c,0,-s)
//   Z: (1,0,0) -> (c,s,0)  and (0,1,0) -> (-s,c,0)
Vec3 RotateXByHand(const Vec3 p, const float a) {
    const float c = std::cos(a);
    const float s = std::sin(a);
    return {p.x, p.y * c - p.z * s, p.y * s + p.z * c};
}

Vec3 RotateYByHand(const Vec3 p, const float a) {
    const float c = std::cos(a);
    const float s = std::sin(a);
    return {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
}

Vec3 RotateZByHand(const Vec3 p, const float a) {
    const float c = std::cos(a);
    const float s = std::sin(a);
    return {p.x * c - p.y * s, p.x * s + p.y * c, p.z};
}

Vec3 ScaleByHand(const Vec3 p, const Vec3 s) { return {p.x * s.x, p.y * s.y, p.z * s.z}; }

bool IsIdentity(const Matrix4& matrix, const float epsilon) {
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            const float expected = row == column ? 1.0F : 0.0F;
            if (!(std::fabs(matrix.values[column * 4 + row] - expected) <= epsilon)) return false;
        }
    }
    return true;
}

bool IsIdentity(const Matrix3& matrix, const float epsilon) {
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t row = 0; row < 3; ++row) {
            const float expected = row == column ? 1.0F : 0.0F;
            if (!(std::fabs(matrix.values[column * 3 + row] - expected) <= epsilon)) return false;
        }
    }
    return true;
}

bool SameValues(const Matrix4& a, const Matrix4& b) { return a.values == b.values; }

} // namespace

#define CHECK_VEC2_EQ(actual, ex, ey)                                                                                  \
    do {                                                                                                               \
        const Vec2 checkValue = (actual);                                                                              \
        CHECK(checkValue.x == (ex));                                                                                   \
        CHECK(checkValue.y == (ey));                                                                                   \
    } while (false)

#define CHECK_VEC3_EQ(actual, ex, ey, ez)                                                                              \
    do {                                                                                                               \
        const Vec3 checkValue = (actual);                                                                              \
        CHECK(checkValue.x == (ex));                                                                                   \
        CHECK(checkValue.y == (ey));                                                                                   \
        CHECK(checkValue.z == (ez));                                                                                   \
    } while (false)

#define CHECK_VEC3_NEAR(actual, expected, eps)                                                                         \
    do {                                                                                                               \
        const Vec3 checkValue = (actual);                                                                              \
        const Vec3 checkExpected = (expected);                                                                         \
        CHECK(checkValue.x == doctest::Approx(checkExpected.x).epsilon(eps));                                          \
        CHECK(checkValue.y == doctest::Approx(checkExpected.y).epsilon(eps));                                          \
        CHECK(checkValue.z == doctest::Approx(checkExpected.z).epsilon(eps));                                          \
    } while (false)

#define CHECK_QUAT_EQ(actual, expected)                                                                                \
    do {                                                                                                               \
        const Quaternion checkValue = (actual);                                                                        \
        const Quaternion checkExpected = (expected);                                                                   \
        CHECK(checkValue.x == checkExpected.x);                                                                        \
        CHECK(checkValue.y == checkExpected.y);                                                                        \
        CHECK(checkValue.z == checkExpected.z);                                                                        \
        CHECK(checkValue.w == checkExpected.w);                                                                        \
    } while (false)

#define CHECK_QUAT_NEAR(actual, expected, eps)                                                                         \
    do {                                                                                                               \
        const Quaternion checkValue = (actual);                                                                        \
        const Quaternion checkExpected = (expected);                                                                   \
        CHECK(checkValue.x == doctest::Approx(checkExpected.x).epsilon(eps));                                          \
        CHECK(checkValue.y == doctest::Approx(checkExpected.y).epsilon(eps));                                          \
        CHECK(checkValue.z == doctest::Approx(checkExpected.z).epsilon(eps));                                          \
        CHECK(checkValue.w == doctest::Approx(checkExpected.w).epsilon(eps));                                          \
    } while (false)

// ---------------------------------------------------------------------------
// scalar/Scalar.hpp, scalar/Constants.hpp
// ---------------------------------------------------------------------------

// Min, Max and Clamp accept every arithmetic type except bool, but all
// arguments must share one type: a mixed call does not compile, so a call site
// cannot convert silently.
template <class A, class B>
concept MinMaxAccepts = requires(A a, B b) {
    Min(a, b);
    Max(a, b);
};
template <class A, class B, class C>
concept ClampAccepts = requires(A a, B b, C c) { Clamp(a, b, c); };

static_assert(MinMaxAccepts<float, float>);
static_assert(MinMaxAccepts<const float&, float>);
static_assert(MinMaxAccepts<double, double>);
static_assert(MinMaxAccepts<int, int>);
static_assert(MinMaxAccepts<std::size_t, std::size_t>);
static_assert(MinMaxAccepts<std::uint64_t, std::uint64_t>);
static_assert(!MinMaxAccepts<float, double>);
static_assert(!MinMaxAccepts<float, int>);
static_assert(!MinMaxAccepts<int, unsigned>);
static_assert(!MinMaxAccepts<bool, bool>);
static_assert(MinMaxAccepts<Vec3, Vec3>);
static_assert(ClampAccepts<float, float, float>);
static_assert(ClampAccepts<double, double, double>);
static_assert(ClampAccepts<std::int32_t, std::int32_t, std::int32_t>);
static_assert(!ClampAccepts<float, float, double>);
static_assert(!ClampAccepts<double, double, float>);
static_assert(ClampAccepts<Vec3, Vec3, Vec3>);
static_assert(ClampAccepts<Vec3d, Vec3d, Vec3d>);
static_assert(Min(3, -2) == -2 && Max(3, -2) == 3 && Clamp(9, 0, 5) == 5);

namespace {

// Min, Max and Clamp must give exactly the std::min, std::max and std::clamp
// results, including which argument a tie or NaN returns (visible as the sign
// of a zero or the payload of a NaN).
template <class T>
[[nodiscard]] bool SameRepresentation(const T a, const T b) noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        if constexpr (sizeof(T) == 4) return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
        else return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
    } else {
        return a == b;
    }
}

template <class T>
void CheckMatchesStd(const std::vector<T>& values) {
    for (const T a : values) {
        for (const T b : values) {
            volatile T va = a;
            volatile T vb = b;
            CHECK(SameRepresentation(Min(T{va}, T{vb}), (std::min)(T{va}, T{vb})));
            CHECK(SameRepresentation(Max(T{va}, T{vb}), (std::max)(T{va}, T{vb})));
            for (const T c : values) {
                if (c < b) continue; // std::clamp requires !(hi < lo)
                volatile T vc = c;
                CHECK(SameRepresentation(Clamp(T{va}, T{vb}, T{vc}), std::clamp(T{va}, T{vb}, T{vc})));
            }
        }
    }
}

} // namespace

TEST_CASE("scalar Min, Max and Clamp match std for float, double and integer types") {
    const float nanF = std::numeric_limits<float>::quiet_NaN();
    const float payloadF = std::bit_cast<float>(0x7FC12345U);
    CheckMatchesStd<float>({0.0F, -0.0F, 1.0F, -1.0F, 2.5F, 1.0e-40F, -1.0e-40F, kInf, -kInf, nanF, payloadF,
                            std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()});
    const double nanD = std::numeric_limits<double>::quiet_NaN();
    CheckMatchesStd<double>({0.0, -0.0, 1.0, -1.0, 2.5, 1.0e-310, std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity(), nanD,
                             std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest()});
    CheckMatchesStd<std::int32_t>({0, 1, -1, 7, std::numeric_limits<std::int32_t>::max(),
                                   std::numeric_limits<std::int32_t>::min()});
    CheckMatchesStd<std::uint64_t>({0, 1, 2, 1000, std::numeric_limits<std::uint64_t>::max()});
    CheckMatchesStd<std::size_t>({0, 1, 64, std::numeric_limits<std::size_t>::max()});
}

TEST_CASE("scalar Clamp, Lerp, Min and Max") {
    CHECK(Min(2.0F, -3.0F) == -3.0F);
    CHECK(Max(2.0F, -3.0F) == 2.0F);
    CHECK(Min(-kInf, 1.0F) == -kInf);
    CHECK(Max(kInf, 1.0F) == kInf);

    CHECK(Clamp(0.5F, 0.0F, 1.0F) == 0.5F);
    CHECK(Clamp(-2.0F, 0.0F, 1.0F) == 0.0F);
    CHECK(Clamp(7.0F, 0.0F, 1.0F) == 1.0F);
    CHECK(Clamp(0.0F, 0.0F, 1.0F) == 0.0F);
    CHECK(Clamp(1.0F, 0.0F, 1.0F) == 1.0F);
    CHECK(Clamp(kInf, -1.0F, 1.0F) == 1.0F);
    CHECK(Clamp(-kInf, -1.0F, 1.0F) == -1.0F);
    CHECK(Clamp(1.0e30F, -1.0F, 1.0F) == 1.0F);
    // Documented: NaN input is returned unchanged.
    CHECK(std::isnan(Clamp(kNaN, 0.0F, 1.0F)));

    // Endpoints and midpoint on exactly representable values.
    CHECK(Lerp(2.0F, 6.0F, 0.0F) == 2.0F);
    CHECK(Lerp(2.0F, 6.0F, 1.0F) == 6.0F);
    CHECK(Lerp(2.0F, 6.0F, 0.5F) == 4.0F);
    // Unclamped: t outside [0, 1] extrapolates.
    CHECK(Lerp(2.0F, 6.0F, 1.5F) == 8.0F);
    CHECK(Lerp(2.0F, 6.0F, -0.5F) == 0.0F);
    CHECK(Lerp(-1.0F, 1.0F, 0.25F) == -0.5F);
}

TEST_CASE("constants") {
    CHECK(Pi == std::numbers::pi_v<float>);
    CHECK(TwoPi == 2.0F * std::numbers::pi_v<float>);
    // Scaling by powers of two is exact.
    CHECK(HalfPi == std::numbers::pi_v<float> / 2.0F);
    CHECK(static_cast<double>(Pi) == doctest::Approx(std::numbers::pi).epsilon(1e-7));
    CHECK(static_cast<double>(TwoPi) == doctest::Approx(2.0 * std::numbers::pi).epsilon(1e-7));
    CHECK(static_cast<double>(HalfPi) == doctest::Approx(0.5 * std::numbers::pi).epsilon(1e-7));
}

// ---------------------------------------------------------------------------
// scalar/Angle.hpp
// ---------------------------------------------------------------------------

TEST_CASE("degree and radian conversion") {
    CHECK(DegreesToRadians(0.0F) == 0.0F);
    CHECK(DegreesToRadians(180.0F) == doctest::Approx(Pi).epsilon(1e-6));
    CHECK(DegreesToRadians(90.0F) == doctest::Approx(HalfPi).epsilon(1e-6));
    CHECK(DegreesToRadians(-45.0F) == doctest::Approx(-0.785398163F).epsilon(1e-6));
    CHECK(DegreesToRadians(360.0F) == doctest::Approx(TwoPi).epsilon(1e-6));
    CHECK(RadiansToDegrees(0.0F) == 0.0F);
    CHECK(RadiansToDegrees(Pi) == doctest::Approx(180.0F).epsilon(1e-6));
    CHECK(RadiansToDegrees(-HalfPi) == doctest::Approx(-90.0F).epsilon(1e-6));
    CHECK(RadiansToDegrees(1.0F) == doctest::Approx(57.2957795F).epsilon(1e-6));

    Lcg random{12345U};
    for (int i = 0; i < 1000; ++i) {
        const float degrees = random.Range(-720.0F, 720.0F);
        CHECK(RadiansToDegrees(DegreesToRadians(degrees)) == doctest::Approx(degrees).epsilon(1e-5));
    }
}

// The conversions are constexpr so products can declare angle constants. Each
// is one multiplication, so constant evaluation must give the run-time bits.
// The run-time inputs go through volatile so the compiler cannot fold them.
namespace {

constexpr std::size_t kAngleSamples = 2881;

[[nodiscard]] constexpr float AngleSample(const std::size_t index) noexcept {
    return static_cast<float>(index) * 0.5F - 720.0F;
}

constexpr auto kConstantRadians = [] {
    std::array<float, kAngleSamples> values{};
    for (std::size_t i = 0; i < values.size(); ++i) values[i] = DegreesToRadians(AngleSample(i));
    return values;
}();

constexpr auto kConstantDegrees = [] {
    std::array<float, kAngleSamples> values{};
    for (std::size_t i = 0; i < values.size(); ++i) values[i] = RadiansToDegrees(AngleSample(i) * 0.01F);
    return values;
}();

} // namespace

static_assert(DegreesToRadians(0.0F) == 0.0F);
static_assert(RadiansToDegrees(0.0F) == 0.0F);

TEST_CASE("degree and radian conversion is constant-evaluable with run-time bits") {
    for (std::size_t i = 0; i < kAngleSamples; ++i) {
        volatile float degrees = AngleSample(i);
        volatile float radians = AngleSample(i) * 0.01F;
        CAPTURE(i);
        CHECK(std::bit_cast<std::uint32_t>(DegreesToRadians(degrees)) ==
              std::bit_cast<std::uint32_t>(kConstantRadians[i]));
        CHECK(std::bit_cast<std::uint32_t>(RadiansToDegrees(radians)) ==
              std::bit_cast<std::uint32_t>(kConstantDegrees[i]));
    }
}

TEST_CASE("WrapRadians stays in [-Pi, Pi] and preserves the angle") {
    CHECK(WrapRadians(0.0F) == 0.0F);
    // Values already inside (-Pi, Pi) are returned exactly.
    CHECK(WrapRadians(-HalfPi) == -HalfPi);
    CHECK(WrapRadians(HalfPi) == HalfPi);
    CHECK(WrapRadians(1.0F) == 1.0F);
    CHECK(WrapRadians(-3.0F) == -3.0F);
    CHECK(WrapRadians(TwoPi) == 0.0F);
    CHECK(WrapRadians(-TwoPi) == 0.0F);
    // The seam: +-Pi are the same angle; either sign is acceptable.
    CHECK(std::fabs(WrapRadians(Pi)) == doctest::Approx(Pi).epsilon(1e-6));
    CHECK(std::fabs(WrapRadians(3.0F * Pi)) == doctest::Approx(Pi).epsilon(1e-6));
    CHECK(std::fabs(WrapRadians(-3.0F * Pi)) == doctest::Approx(Pi).epsilon(1e-6));
    // 2Pi + 1 -> 1, -2Pi - 1 -> -1.
    CHECK(WrapRadians(TwoPi + 1.0F) == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK(WrapRadians(-TwoPi - 1.0F) == doctest::Approx(-1.0F).epsilon(1e-6));
    CHECK(WrapRadians(5.0F * TwoPi + 0.5F) == doctest::Approx(0.5F).epsilon(1e-5));

    std::vector<float> inputs{0.0F, -0.0F, kDenormMin, -kDenormMin, kFloatMin, Pi, -Pi, TwoPi,
                              1.0e19F, -1.0e19F, 1.0e30F, -1.0e30F,
                              (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::lowest)()};
    Lcg random{777U};
    for (int i = 0; i < 2000; ++i) inputs.push_back(random.Range(-1000.0F, 1000.0F));
    for (const float input : inputs) {
        CAPTURE(input);
        const float wrapped = WrapRadians(input);
        CHECK(wrapped >= -Pi);
        CHECK(wrapped <= Pi);
    }
    // For moderate inputs the wrapped value is the same angle. The tolerance
    // allows for the float TwoPi differing from 2*pi (|n| * 1.7e-7).
    for (std::size_t i = 14; i < inputs.size(); ++i) {
        const float input = inputs[i];
        CAPTURE(input);
        const float wrapped = WrapRadians(input);
        CHECK(std::cos(wrapped) == doctest::Approx(std::cos(input)).epsilon(1e-4));
        CHECK(std::sin(wrapped) == doctest::Approx(std::sin(input)).epsilon(1e-4));
    }
    // Tiny values are inside the range and untouched.
    CHECK(WrapRadians(kDenormMin) == kDenormMin);
    CHECK(WrapRadians(-kFloatMin) == -kFloatMin);
}

TEST_CASE("LerpRadiansShortest takes the short arc across the seam") {
    // Without a seam crossing it is an ordinary lerp.
    CHECK(LerpRadiansShortest(0.5F, 1.5F, 0.5F) == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK(LerpRadiansShortest(0.5F, 1.5F, 0.0F) == doctest::Approx(0.5F).epsilon(1e-6));
    CHECK(LerpRadiansShortest(0.5F, 1.5F, 1.0F) == doctest::Approx(1.5F).epsilon(1e-6));
    CHECK(LerpRadiansShortest(-1.0F, 1.0F, 0.25F) == doctest::Approx(-0.5F).epsilon(1e-6));

    // 3 -> -3 rad: the short arc has length 2*pi - 6 ~= 0.2832 and passes +Pi.
    const float arc = static_cast<float>(2.0 * std::numbers::pi - 6.0);
    CHECK(LerpRadiansShortest(3.0F, -3.0F, 0.0F) == doctest::Approx(3.0F).epsilon(1e-6));
    CHECK(LerpRadiansShortest(3.0F, -3.0F, 0.25F) == doctest::Approx(3.0F + 0.25F * arc).epsilon(1e-5));
    CHECK(std::fabs(LerpRadiansShortest(3.0F, -3.0F, 0.5F)) == doctest::Approx(Pi).epsilon(1e-5));
    CHECK(LerpRadiansShortest(3.0F, -3.0F, 0.75F) ==
          doctest::Approx(-3.0F - 0.25F * arc).epsilon(1e-5));
    CHECK(LerpRadiansShortest(3.0F, -3.0F, 1.0F) == doctest::Approx(-3.0F).epsilon(1e-5));
    // Reverse direction goes the other way round.
    CHECK(LerpRadiansShortest(-3.0F, 3.0F, 0.25F) == doctest::Approx(-3.0F - 0.25F * arc).epsilon(1e-5));
    // Inputs outside [-Pi, Pi] give a wrapped result.
    CHECK(LerpRadiansShortest(TwoPi + 0.5F, 1.5F, 0.5F) == doctest::Approx(1.0F).epsilon(1e-5));

    Lcg random{4242U};
    for (int i = 0; i < 500; ++i) {
        const float a = random.Range(-Pi, Pi);
        const float b = random.Range(-Pi, Pi);
        const float t = random.Range(0.0F, 1.0F);
        const float result = LerpRadiansShortest(a, b, t);
        CAPTURE(a);
        CAPTURE(b);
        CAPTURE(t);
        CHECK(result >= -Pi);
        CHECK(result <= Pi);
        // Short arc: the travelled angle from a never exceeds Pi * t.
        const float travelled = std::fabs(WrapRadians(result - a));
        CHECK(travelled <= Pi * t + 1.0e-4F);
    }
}

// ---------------------------------------------------------------------------
// scalar/ColorSpace.hpp
// ---------------------------------------------------------------------------

TEST_CASE("sRGB transfer functions") {
    CHECK(DecodeSrgb(0.0F) == 0.0F);
    CHECK(DecodeSrgb(1.0F) == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK(EncodeSrgb(0.0F) == 0.0F);
    CHECK(EncodeSrgb(1.0F) == doctest::Approx(1.0F).epsilon(1e-6));
    // ((0.5 + 0.055) / 1.055)^2.4 = 0.2140411...
    CHECK(DecodeSrgb(0.5F) == doctest::Approx(0.214041F).epsilon(1e-5));
    CHECK(EncodeSrgb(0.214041F) == doctest::Approx(0.5F).epsilon(1e-5));
    // Linear segment: encoded / 12.92 and linear * 12.92.
    CHECK(DecodeSrgb(0.04F) == doctest::Approx(0.04F / 12.92F).epsilon(1e-6));
    CHECK(EncodeSrgb(0.002F) == doctest::Approx(0.02584F).epsilon(1e-6));
    // The two pieces meet at the thresholds.
    CHECK(DecodeSrgb(0.04045F) == doctest::Approx(0.0031308F).epsilon(1e-6));
    CHECK(EncodeSrgb(0.0031308F) == doctest::Approx(0.04045F).epsilon(1e-6));

    // Out-of-range input is clamped to [0, 1].
    CHECK(DecodeSrgb(-0.5F) == 0.0F);
    CHECK(DecodeSrgb(-kInf) == 0.0F);
    CHECK(DecodeSrgb(2.0F) == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK(DecodeSrgb(kInf) == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK(EncodeSrgb(-1.0F) == 0.0F);
    CHECK(EncodeSrgb(5.0F) == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK(EncodeSrgb(1.0e30F) == doctest::Approx(1.0F).epsilon(1e-6));

    // Round trip and monotonicity over every 8-bit value.
    float previous = -1.0F;
    for (int i = 0; i <= 255; ++i) {
        const float encoded = static_cast<float>(i) / 255.0F;
        const float linear = DecodeSrgb(encoded);
        CAPTURE(i);
        CHECK(linear > previous);
        CHECK(linear >= 0.0F);
        CHECK(linear <= 1.0F + 1.0e-6F);
        previous = linear;
        const float back = EncodeSrgb(linear);
        CHECK(back == doctest::Approx(encoded).epsilon(1e-5));
        CHECK(static_cast<int>(std::lround(back * 255.0F)) == i);
    }
}

// ---------------------------------------------------------------------------
// linear/Vec2.hpp, Vec3.hpp, Vec4.hpp
// ---------------------------------------------------------------------------

TEST_CASE("vector defaults are zero") {
    const Vec2 v2{};
    const Vec3 v3{};
    const Vec4 v4{};
    CHECK(v2.x == 0.0F);
    CHECK(v2.y == 0.0F);
    CHECK(v3.x == 0.0F);
    CHECK(v3.y == 0.0F);
    CHECK(v3.z == 0.0F);
    CHECK(v4.x == 0.0F);
    CHECK(v4.y == 0.0F);
    CHECK(v4.z == 0.0F);
    CHECK(v4.w == 0.0F);
}

TEST_CASE("Vec2 operations") {
    const Vec2 a{1.0F, 2.0F};
    const Vec2 b{3.0F, -4.0F};
    CHECK_VEC2_EQ(a + b, 4.0F, -2.0F);
    CHECK_VEC2_EQ(a - b, -2.0F, 6.0F);
    CHECK_VEC2_EQ(-a, -1.0F, -2.0F);
    CHECK_VEC2_EQ(a * 2.0F, 2.0F, 4.0F);
    CHECK_VEC2_EQ(2.0F * a, 2.0F, 4.0F);
    CHECK_VEC2_EQ(b / 2.0F, 1.5F, -2.0F);
    CHECK(Dot(a, b) == -5.0F);
    CHECK(LengthSquared(b) == 25.0F);
    CHECK(Length(b) == 5.0F);
    CHECK(Distance(a, Vec2{4.0F, 6.0F}) == 5.0F);
    CHECK_VEC2_EQ(Normalize(Vec2{0.0F, -3.0F}), 0.0F, -1.0F);
    CHECK(Length(Normalize(b)) == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK_VEC2_EQ(NormalizeOrZero(Vec2{}), 0.0F, 0.0F);
    CHECK_VEC2_EQ(NormalizeOrZero(Vec2{3.0F, 4.0F}), 0.6F, 0.8F);
    CHECK_VEC2_EQ(Lerp(a, b, 0.0F), 1.0F, 2.0F);
    CHECK_VEC2_EQ(Lerp(a, b, 1.0F), 3.0F, -4.0F);
    CHECK_VEC2_EQ(Lerp(a, b, 0.5F), 2.0F, -1.0F);
    CHECK_VEC2_EQ(Lerp(a, b, 2.0F), 5.0F, -10.0F);
    CHECK_VEC2_EQ(Min(a, b), 1.0F, -4.0F);
    CHECK_VEC2_EQ(Max(a, b), 3.0F, 2.0F);
    CHECK_VEC2_EQ(Clamp(Vec2{-5.0F, 5.0F}, Vec2{0.0F, 0.0F}, Vec2{1.0F, 2.0F}), 0.0F, 2.0F);
    CHECK_VEC2_EQ(Clamp(Vec2{0.5F, 1.5F}, Vec2{0.0F, 0.0F}, Vec2{1.0F, 2.0F}), 0.5F, 1.5F);
    CHECK(IsFinite(a));
    CHECK_FALSE(IsFinite(Vec2{kInf, 0.0F}));
    CHECK_FALSE(IsFinite(Vec2{0.0F, kNaN}));
    CHECK_FALSE(IsFinite(Vec2{-kInf, 0.0F}));
}

TEST_CASE("Vec3 arithmetic, Dot and Cross") {
    const Vec3 a{1.0F, 2.0F, 3.0F};
    const Vec3 b{-4.0F, 5.0F, 0.5F};
    CHECK_VEC3_EQ(a + b, -3.0F, 7.0F, 3.5F);
    CHECK_VEC3_EQ(a - b, 5.0F, -3.0F, 2.5F);
    CHECK_VEC3_EQ(-a, -1.0F, -2.0F, -3.0F);
    CHECK_VEC3_EQ(a * 2.0F, 2.0F, 4.0F, 6.0F);
    CHECK_VEC3_EQ(0.5F * a, 0.5F, 1.0F, 1.5F);
    CHECK_VEC3_EQ(a / 4.0F, 0.25F, 0.5F, 0.75F);
    CHECK(Dot(a, b) == 7.5F);
    CHECK(Dot(Vec3{1.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}) == 0.0F);

    // Cross is the algebraic product; with the basis X right, Y up, Z forward
    // these identities hold regardless of handedness.
    const Vec3 x{1.0F, 0.0F, 0.0F};
    const Vec3 y{0.0F, 1.0F, 0.0F};
    const Vec3 z{0.0F, 0.0F, 1.0F};
    CHECK_VEC3_EQ(Cross(x, y), 0.0F, 0.0F, 1.0F);
    CHECK_VEC3_EQ(Cross(y, z), 1.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(Cross(z, x), 0.0F, 1.0F, 0.0F);
    CHECK_VEC3_EQ(Cross(y, x), 0.0F, 0.0F, -1.0F);
    CHECK_VEC3_EQ(Cross(x, x), 0.0F, 0.0F, 0.0F);
    // (1,2,3) x (-4,5,0.5) = (2*0.5 - 3*5, 3*(-4) - 1*0.5, 1*5 - 2*(-4))
    CHECK_VEC3_EQ(Cross(a, b), -14.0F, -12.5F, 13.0F);

    Lcg random{99U};
    for (int i = 0; i < 500; ++i) {
        const Vec3 p = random.RangeVec3(-10.0F, 10.0F);
        const Vec3 q = random.RangeVec3(-10.0F, 10.0F);
        const Vec3 c = Cross(p, q);
        // Perpendicular to both inputs and anti-commutative. Each component is a
        // difference of products, so rounding (and FMA contraction, which rounds
        // one product exactly) scales with |p||q|, not with the result.
        const float scale = Length(p) * Length(q);
        CHECK(std::fabs(Dot(c, p)) <= 1e-5F * scale * Length(p));
        CHECK(std::fabs(Dot(c, q)) <= 1e-5F * scale * Length(q));
        const Vec3 reversed = Cross(q, p);
        CHECK(std::fabs(reversed.x + c.x) <= 1e-6F * scale);
        CHECK(std::fabs(reversed.y + c.y) <= 1e-6F * scale);
        CHECK(std::fabs(reversed.z + c.z) <= 1e-6F * scale);
        CHECK(Dot(p, q) == Dot(q, p));
    }
}

TEST_CASE("Vec3 length, distance and normalization") {
    CHECK(LengthSquared(Vec3{2.0F, 3.0F, 6.0F}) == 49.0F);
    CHECK(Length(Vec3{2.0F, 3.0F, 6.0F}) == 7.0F);
    CHECK(Distance(Vec3{1.0F, 1.0F, 1.0F}, Vec3{3.0F, 4.0F, 7.0F}) == 7.0F);
    CHECK(Distance(Vec3{3.0F, 4.0F, 7.0F}, Vec3{1.0F, 1.0F, 1.0F}) == 7.0F);
    CHECK(Length(Vec3{}) == 0.0F);
    CHECK_VEC3_EQ(Normalize(Vec3{0.0F, 0.0F, -2.0F}), 0.0F, 0.0F, -1.0F);
    CHECK_VEC3_EQ(NormalizeOrZero(Vec3{}), 0.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(NormalizeOrZero(Vec3{0.0F, 4.0F, 3.0F}), 0.0F, 0.8F, 0.6F);
    // Documented: Normalize does not guard a zero vector.
    CHECK_FALSE(IsFinite(Normalize(Vec3{})));

    // Unit length over a grid that includes very large and very small scales.
    Lcg random{2024U};
    for (int i = 0; i < 1000; ++i) {
        const float scale = std::pow(10.0F, random.Range(-12.0F, 18.0F));
        const Vec3 v = random.RangeVec3(-1.0F, 1.0F) * scale;
        if (LengthSquared(v) == 0.0F) continue;
        CAPTURE(v.x);
        CAPTURE(v.y);
        CAPTURE(v.z);
        CHECK(Length(Normalize(v)) == doctest::Approx(1.0F).epsilon(1e-6));
        CHECK(Length(NormalizeOrZero(v)) == doctest::Approx(1.0F).epsilon(1e-6));
        // Direction is preserved.
        CHECK(Dot(Normalize(v), v) > 0.0F);
    }
    CHECK(Length(Vec3{1.0e19F, 1.0e19F, 1.0e19F}) == doctest::Approx(1.7320508e19F).epsilon(1e-6));
    CHECK(Length(Normalize(Vec3{1.0e19F, -1.0e19F, 1.0e19F})) == doctest::Approx(1.0F).epsilon(1e-6));

    // Documented limit: Length is sqrt of the sum of squares (not hypot) and
    // overflows above ~1.8e19 per component.
    CHECK(Length(Vec3{1.0e30F, 0.0F, 0.0F}) == kInf);
    // NormalizeOrZero never produces non-finite output for finite input, even
    // when the squared length underflows or overflows.
    for (const float s : {kDenormMin, kFloatMin, 1.0e-30F, 1.0e30F, (std::numeric_limits<float>::max)()}) {
        CAPTURE(s);
        CHECK(IsFinite(NormalizeOrZero(Vec3{s, -s, s})));
        CHECK(IsFinite(NormalizeOrZero(Vec2{s, -s})));
        CHECK(IsFinite(NormalizeOrZero(Vec4{s, -s, s, -s})));
    }
}

TEST_CASE("Vec3 Lerp, Min, Max, Clamp and IsFinite") {
    const Vec3 a{0.0F, 2.0F, -4.0F};
    const Vec3 b{4.0F, -2.0F, 4.0F};
    CHECK_VEC3_EQ(Lerp(a, b, 0.0F), 0.0F, 2.0F, -4.0F);
    CHECK_VEC3_EQ(Lerp(a, b, 1.0F), 4.0F, -2.0F, 4.0F);
    CHECK_VEC3_EQ(Lerp(a, b, 0.25F), 1.0F, 1.0F, -2.0F);
    CHECK_VEC3_EQ(Lerp(a, b, -1.0F), -4.0F, 6.0F, -12.0F);
    CHECK_VEC3_EQ(Lerp(a, b, 1.5F), 6.0F, -4.0F, 8.0F);

    CHECK_VEC3_EQ(Min(a, b), 0.0F, -2.0F, -4.0F);
    CHECK_VEC3_EQ(Max(a, b), 4.0F, 2.0F, 4.0F);
    const Vec3 lo{-1.0F, -1.0F, -1.0F};
    const Vec3 hi{1.0F, 1.0F, 1.0F};
    CHECK_VEC3_EQ(Clamp(Vec3{-3.0F, 0.5F, 9.0F}, lo, hi), -1.0F, 0.5F, 1.0F);
    CHECK_VEC3_EQ(Clamp(Vec3{-kInf, kInf, 0.0F}, lo, hi), -1.0F, 1.0F, 0.0F);

    CHECK(IsFinite(a));
    CHECK(IsFinite(Vec3{(std::numeric_limits<float>::max)(), kDenormMin, -0.0F}));
    CHECK_FALSE(IsFinite(Vec3{kInf, 0.0F, 0.0F}));
    CHECK_FALSE(IsFinite(Vec3{0.0F, -kInf, 0.0F}));
    CHECK_FALSE(IsFinite(Vec3{0.0F, 0.0F, kNaN}));
}

TEST_CASE("Vec4 operations and homogeneous helpers") {
    const Vec4 a{1.0F, 2.0F, 3.0F, 4.0F};
    const Vec4 b{4.0F, 3.0F, 2.0F, 1.0F};
    const Vec4 sum = a + b;
    CHECK(sum.x == 5.0F);
    CHECK(sum.w == 5.0F);
    const Vec4 difference = a - b;
    CHECK(difference.x == -3.0F);
    CHECK(difference.w == 3.0F);
    const Vec4 negated = -a;
    CHECK(negated.y == -2.0F);
    CHECK(negated.w == -4.0F);
    CHECK((a * 2.0F).z == 6.0F);
    CHECK((2.0F * a).w == 8.0F);
    CHECK((a / 2.0F).x == 0.5F);
    CHECK(Dot(a, b) == 20.0F);
    CHECK(LengthSquared(a) == 30.0F);
    CHECK(Length(Vec4{1.0F, 1.0F, 1.0F, 1.0F}) == 2.0F);
    CHECK(Distance(Vec4{}, Vec4{2.0F, 2.0F, 2.0F, 2.0F}) == 4.0F);
    const Vec4 unit = Normalize(Vec4{0.0F, 0.0F, 0.0F, -5.0F});
    CHECK(unit.w == -1.0F);
    CHECK(Length(Normalize(a)) == doctest::Approx(1.0F).epsilon(1e-6));
    const Vec4 zero = NormalizeOrZero(Vec4{});
    CHECK(zero.x == 0.0F);
    CHECK(zero.w == 0.0F);
    const Vec4 mid = Lerp(a, b, 0.5F);
    CHECK(mid.x == 2.5F);
    CHECK(mid.w == 2.5F);
    const Vec4 extrapolated = Lerp(a, b, 2.0F);
    CHECK(extrapolated.x == 7.0F);
    CHECK(extrapolated.w == -2.0F);
    const Vec4 lower = Min(a, b);
    const Vec4 upper = Max(a, b);
    CHECK(lower.x == 1.0F);
    CHECK(lower.w == 1.0F);
    CHECK(upper.x == 4.0F);
    CHECK(upper.w == 4.0F);
    const Vec4 clamped =
        Clamp(Vec4{-1.0F, 9.0F, 2.5F, 0.0F}, Vec4{0.0F, 0.0F, 0.0F, 0.0F}, Vec4{3.0F, 3.0F, 3.0F, 3.0F});
    CHECK(clamped.x == 0.0F);
    CHECK(clamped.y == 3.0F);
    CHECK(clamped.z == 2.5F);
    CHECK(clamped.w == 0.0F);
    CHECK(IsFinite(a));
    CHECK_FALSE(IsFinite(Vec4{0.0F, 0.0F, 0.0F, kNaN}));
    CHECK_FALSE(IsFinite(Vec4{kInf, 0.0F, 0.0F, 0.0F}));

    const Vec4 point = MakePoint4({1.0F, 2.0F, 3.0F});
    CHECK(point.w == 1.0F);
    const Vec4 direction = MakeDirection4({1.0F, 2.0F, 3.0F});
    CHECK(direction.w == 0.0F);
    CHECK_VEC3_EQ(XYZ(point), 1.0F, 2.0F, 3.0F);
    CHECK_VEC3_EQ(XYZ(direction), 1.0F, 2.0F, 3.0F);
}

// ---------------------------------------------------------------------------
// linear/VecInt.hpp, linear/Vec3d.hpp
// ---------------------------------------------------------------------------

TEST_CASE("integer vectors compare exactly and add") {
    static_assert(Vec2i{1, 2} + Vec2i{3, 4} == Vec2i{4, 6});
    static_assert(Vec3i{1, 2, 3} - Vec3i{3, 2, 1} == Vec3i{-2, 0, 2});

    const Vec2i a{3, -7};
    const Vec2i b{-1, 2};
    CHECK(a == Vec2i{3, -7});
    CHECK_FALSE(a == b);
    CHECK(a != b);
    CHECK(a + b == Vec2i{2, -5});
    CHECK(a - b == Vec2i{4, -9});
    CHECK(-a == Vec2i{-3, 7});
    CHECK(Vec2i{} == Vec2i{0, 0});

    const Vec3i c{1, 2, 3};
    const Vec3i d{10, -20, 30};
    CHECK(c == Vec3i{1, 2, 3});
    CHECK(c != d);
    CHECK_FALSE(c == Vec3i{1, 2, 4});
    CHECK(c + d == Vec3i{11, -18, 33});
    CHECK(c - d == Vec3i{-9, 22, -27});
    CHECK(-c == Vec3i{-1, -2, -3});
    CHECK(Vec3i{} == Vec3i{0, 0, 0});
    const std::int32_t large = (std::numeric_limits<std::int32_t>::max)();
    CHECK(Vec2i{large, -large} - Vec2i{large, -large} == Vec2i{});
}

TEST_CASE("Vec3d operations and conversions") {
    const Vec3d a{1.0, 2.0, 3.0};
    const Vec3d b{-4.0, 5.0, 0.5};
    const Vec3d sum = a + b;
    CHECK(sum.x == -3.0);
    CHECK(sum.z == 3.5);
    const Vec3d difference = a - b;
    CHECK(difference.y == -3.0);
    CHECK((-a).z == -3.0);
    CHECK((a * 2.0).y == 4.0);
    CHECK((2.0 * a).x == 2.0);
    CHECK((a / 2.0).z == 1.5);
    CHECK(Dot(a, b) == 7.5);
    const Vec3d cross = Cross(Vec3d{1.0, 0.0, 0.0}, Vec3d{0.0, 1.0, 0.0});
    CHECK(cross.x == 0.0);
    CHECK(cross.y == 0.0);
    CHECK(cross.z == 1.0);
    const Vec3d crossAb = Cross(a, b);
    CHECK(crossAb.x == -14.0);
    CHECK(crossAb.y == -12.5);
    CHECK(crossAb.z == 13.0);
    CHECK(LengthSquared(Vec3d{2.0, 3.0, 6.0}) == 49.0);
    CHECK(Length(Vec3d{2.0, 3.0, 6.0}) == 7.0);
    const Vec3d clamped = Clamp(Vec3d{-2.0, 0.5, 9.0}, Vec3d{-1.0, -1.0, -1.0}, Vec3d{1.0, 1.0, 1.0});
    CHECK(clamped.x == -1.0);
    CHECK(clamped.y == 0.5);
    CHECK(clamped.z == 1.0);
    CHECK(IsFinite(a));
    CHECK_FALSE(IsFinite(Vec3d{std::numeric_limits<double>::infinity(), 0.0, 0.0}));
    CHECK_FALSE(IsFinite(Vec3d{0.0, std::numeric_limits<double>::quiet_NaN(), 0.0}));

    // float -> double -> float is exact for every float, special values included.
    const std::vector<float> specials{0.0F, -0.0F, kDenormMin, -kDenormMin, kFloatMin, 1.0e19F, -1.0e30F,
                                      (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::lowest)(),
                                      kInf, -kInf, 0.1F, 1.0F / 3.0F};
    for (const float s : specials) {
        CAPTURE(s);
        const Vec3 v{s, -s, s * 0.5F};
        const Vec3d wide = ToVec3d(v);
        CHECK(wide.x == static_cast<double>(v.x));
        CHECK(wide.y == static_cast<double>(v.y));
        CHECK(wide.z == static_cast<double>(v.z));
        const Vec3 back = ToVec3(wide);
        CHECK(back.x == v.x);
        CHECK(back.y == v.y);
        CHECK(back.z == v.z);
        CHECK(std::signbit(back.x) == std::signbit(v.x));
    }
    const Vec3 nanBack = ToVec3(ToVec3d(Vec3{kNaN, 0.0F, 0.0F}));
    CHECK(std::isnan(nanBack.x));
    Lcg random{31337U};
    for (int i = 0; i < 500; ++i) {
        const Vec3 v = random.RangeVec3(-1.0e6F, 1.0e6F);
        const Vec3 back = ToVec3(ToVec3d(v));
        CHECK(back.x == v.x);
        CHECK(back.y == v.y);
        CHECK(back.z == v.z);
    }
    // Narrowing rounds to nearest.
    CHECK(ToVec3(Vec3d{0.1, 0.0, 0.0}).x == 0.1F);
}

// ---------------------------------------------------------------------------
// linear/Matrix4.hpp: defaults, layout and conventions
// ---------------------------------------------------------------------------

TEST_CASE("Matrix4 defaults: identity, Matrix4{{}} and Zero()") {
    const Matrix4 identity{};
    for (std::size_t i = 0; i < 16; ++i) {
        CAPTURE(i);
        CHECK(identity.values[i] == (i % 5 == 0 ? 1.0F : 0.0F));
    }
    const Matrix4 braced{{}};
    const Matrix4 zero = Zero();
    for (std::size_t i = 0; i < 16; ++i) {
        CAPTURE(i);
        CHECK(braced.values[i] == 0.0F);
        CHECK(zero.values[i] == 0.0F);
    }
    const Matrix3 identity3{};
    for (std::size_t i = 0; i < 9; ++i) {
        CAPTURE(i);
        CHECK(identity3.values[i] == (i % 4 == 0 ? 1.0F : 0.0F));
    }
    CHECK_VEC3_EQ(TransformPoint(identity, Vec3{1.0F, -2.0F, 3.0F}), 1.0F, -2.0F, 3.0F);
}

TEST_CASE("Matrix4 storage is column-major with column vectors") {
    // values[column * 4 + row]: the image of the basis vector e_c is column c.
    Matrix4 m = Zero();
    for (std::size_t i = 0; i < 16; ++i) m.values[i] = static_cast<float>(i + 1);
    // Column 0 holds values[0..3], so TransformVector(m, X) = (1, 2, 3).
    CHECK_VEC3_EQ(TransformVector(m, Vec3{1.0F, 0.0F, 0.0F}), 1.0F, 2.0F, 3.0F);
    CHECK_VEC3_EQ(TransformVector(m, Vec3{0.0F, 1.0F, 0.0F}), 5.0F, 6.0F, 7.0F);
    CHECK_VEC3_EQ(TransformVector(m, Vec3{0.0F, 0.0F, 1.0F}), 9.0F, 10.0F, 11.0F);
    // TransformPoint adds column 3 (w = 1); call with {} must compile.
    CHECK_VEC3_EQ(TransformPoint(m, {}), 13.0F, 14.0F, 15.0F);
    CHECK_VEC3_EQ(TransformPoint(m, Vec3{1.0F, 0.0F, 0.0F}), 14.0F, 16.0F, 18.0F);

    // Translation lives at values[12..14].
    const Matrix4 translation = MakeTranslation({4.0F, -5.0F, 6.0F});
    CHECK(translation.values[12] == 4.0F);
    CHECK(translation.values[13] == -5.0F);
    CHECK(translation.values[14] == 6.0F);
    CHECK(translation.values[15] == 1.0F);
    CHECK(translation.values[3] == 0.0F);
    CHECK(translation.values[7] == 0.0F);
    CHECK(translation.values[11] == 0.0F);
    CHECK_VEC3_EQ(TransformPoint(translation, {}), 4.0F, -5.0F, 6.0F);
    CHECK_VEC3_EQ(TransformPoint(translation, Vec3{1.0F, 1.0F, 1.0F}), 5.0F, -4.0F, 7.0F);
    // Directions ignore translation.
    CHECK_VEC3_EQ(TransformVector(translation, Vec3{1.0F, 1.0F, 1.0F}), 1.0F, 1.0F, 1.0F);

    const Matrix4 scale = MakeScale({2.0F, 3.0F, -4.0F});
    CHECK(scale.values[0] == 2.0F);
    CHECK(scale.values[5] == 3.0F);
    CHECK(scale.values[10] == -4.0F);
    CHECK(scale.values[15] == 1.0F);
    CHECK_VEC3_EQ(TransformPoint(scale, Vec3{1.0F, 1.0F, 1.0F}), 2.0F, 3.0F, -4.0F);
}

TEST_CASE("rotation conventions are left-handed") {
    const std::array<float, 9> angles{0.0F, 0.3F, -0.7F, 1.0F, HalfPi, -HalfPi, 2.5F, Pi, -3.0F};
    for (const float a : angles) {
        CAPTURE(a);
        const float s = std::sin(a);
        const float c = std::cos(a);
        CHECK_VEC3_NEAR(TransformPoint(MakeRotationY(a), Vec3{0.0F, 0.0F, 1.0F}), (Vec3{s, 0.0F, c}), 1e-6);
        CHECK_VEC3_NEAR(TransformPoint(MakeRotationY(a), Vec3{1.0F, 0.0F, 0.0F}), (Vec3{c, 0.0F, -s}), 1e-6);
        CHECK_VEC3_NEAR(TransformPoint(MakeRotationY(a), Vec3{0.0F, 1.0F, 0.0F}), (Vec3{0.0F, 1.0F, 0.0F}), 1e-6);
        CHECK_VEC3_NEAR(TransformPoint(MakeRotationX(a), Vec3{0.0F, 0.0F, 1.0F}), (Vec3{0.0F, -s, c}), 1e-6);
        CHECK_VEC3_NEAR(TransformPoint(MakeRotationX(a), Vec3{0.0F, 1.0F, 0.0F}), (Vec3{0.0F, c, s}), 1e-6);
        CHECK_VEC3_NEAR(TransformPoint(MakeRotationX(a), Vec3{1.0F, 0.0F, 0.0F}), (Vec3{1.0F, 0.0F, 0.0F}), 1e-6);
        CHECK_VEC3_NEAR(TransformPoint(MakeRotationZ(a), Vec3{1.0F, 0.0F, 0.0F}), (Vec3{c, s, 0.0F}), 1e-6);
        CHECK_VEC3_NEAR(TransformPoint(MakeRotationZ(a), Vec3{0.0F, 1.0F, 0.0F}), (Vec3{-s, c, 0.0F}), 1e-6);
        CHECK_VEC3_NEAR(TransformPoint(MakeRotationZ(a), Vec3{0.0F, 0.0F, 1.0F}), (Vec3{0.0F, 0.0F, 1.0F}), 1e-6);
        // Proper rotations: determinant +1, no translation.
        CHECK(Determinant(MakeRotationX(a)) == doctest::Approx(1.0F).epsilon(1e-6));
        CHECK(Determinant(MakeRotationY(a)) == doctest::Approx(1.0F).epsilon(1e-6));
        CHECK(Determinant(MakeRotationZ(a)) == doctest::Approx(1.0F).epsilon(1e-6));
        CHECK_VEC3_EQ(TransformPoint(MakeRotationY(a), {}), 0.0F, 0.0F, 0.0F);
    }
    // Positive yaw turns forward (+Z) toward right (+X).
    CHECK_VEC3_NEAR(TransformPoint(MakeRotationY(HalfPi), Vec3{0.0F, 0.0F, 1.0F}), (Vec3{1.0F, 0.0F, 0.0F}), 1e-6);
    // Positive pitch about +X turns forward (+Z) toward down (-Y).
    CHECK_VEC3_NEAR(TransformPoint(MakeRotationX(HalfPi), Vec3{0.0F, 0.0F, 1.0F}), (Vec3{0.0F, -1.0F, 0.0F}), 1e-6);
    // Positive roll about +Z turns right (+X) toward up (+Y).
    CHECK_VEC3_NEAR(TransformPoint(MakeRotationZ(HalfPi), Vec3{1.0F, 0.0F, 0.0F}), (Vec3{0.0F, 1.0F, 0.0F}), 1e-6);
}

TEST_CASE("Multiply(parent, local) applies local first") {
    const Matrix4 translation = MakeTranslation({10.0F, 0.0F, 0.0F});
    const Matrix4 scale = MakeScale({2.0F, 2.0F, 2.0F});
    // T(S(p)): scale (1,1,1) -> (2,2,2) then translate -> (12,2,2).
    CHECK_VEC3_EQ(TransformPoint(Multiply(translation, scale), Vec3{1.0F, 1.0F, 1.0F}), 12.0F, 2.0F, 2.0F);
    // S(T(p)): translate -> (11,1,1) then scale -> (22,2,2).
    CHECK_VEC3_EQ(TransformPoint(Multiply(scale, translation), Vec3{1.0F, 1.0F, 1.0F}), 22.0F, 2.0F, 2.0F);
    // Exact storage of T * S: scale on the diagonal, translation untouched.
    const Matrix4 ts = Multiply(translation, scale);
    CHECK(ts.values[0] == 2.0F);
    CHECK(ts.values[5] == 2.0F);
    CHECK(ts.values[10] == 2.0F);
    CHECK(ts.values[12] == 10.0F);
    CHECK(ts.values[15] == 1.0F);
    // S * T scales the translation column.
    CHECK(Multiply(scale, translation).values[12] == 20.0F);

    // Rotation then translation: rotate +Z to +X by yaw, then move.
    const Matrix4 yaw = MakeRotationY(HalfPi);
    CHECK_VEC3_NEAR(TransformPoint(Multiply(translation, yaw), Vec3{0.0F, 0.0F, 1.0F}),
                    (Vec3{11.0F, 0.0F, 0.0F}), 1e-6);

    // Identity is neutral on both sides.
    CHECK(SameValues(Multiply(Matrix4{}, ts), ts));
    CHECK(SameValues(Multiply(ts, Matrix4{}), ts));

    // General property on a grid: (A*B)(p) == A(B(p)).
    Lcg random{8U};
    for (int i = 0; i < 200; ++i) {
        const Matrix4 a = RandomEulerTransform(random);
        const Matrix4 b = RandomEulerTransform(random);
        const Vec3 p = random.RangeVec3(-3.0F, 3.0F);
        CHECK_VEC3_NEAR(TransformPoint(Multiply(a, b), p), TransformPoint(a, TransformPoint(b, p)), 1e-5);
        CHECK_VEC3_NEAR(TransformVector(Multiply(a, b), p), TransformVector(a, TransformVector(b, p)), 1e-5);
    }
}

TEST_CASE("ComposeEulerXYZ applies scale, X, Y, Z, then translation") {
    const Vec3 translation{4.0F, -5.0F, 6.0F};
    const Vec3 rotation{0.3F, -1.1F, 2.2F};
    const Vec3 scale{2.0F, 0.5F, -1.5F};
    const Matrix4 m = ComposeEulerXYZ(translation, rotation, scale);
    Lcg random{55U};
    for (int i = 0; i < 200; ++i) {
        const Vec3 p = random.RangeVec3(-10.0F, 10.0F);
        Vec3 expected = ScaleByHand(p, scale);
        expected = RotateXByHand(expected, rotation.x);
        expected = RotateYByHand(expected, rotation.y);
        expected = RotateZByHand(expected, rotation.z);
        expected = expected + translation;
        CHECK_VEC3_NEAR(TransformPoint(m, p), expected, 1e-5);
    }
    // A single axis reduces to the matching elementary rotation.
    const Matrix4 onlyY = ComposeEulerXYZ({}, {0.0F, 0.8F, 0.0F}, {1.0F, 1.0F, 1.0F});
    CHECK_VEC3_NEAR(TransformPoint(onlyY, Vec3{0.0F, 0.0F, 1.0F}), (Vec3{std::sin(0.8F), 0.0F, std::cos(0.8F)}),
                    1e-6);
    // X then Y on +Z: X(90) sends +Z to -Y, which Y leaves alone.
    const Matrix4 xThenY = ComposeEulerXYZ({}, {HalfPi, HalfPi, 0.0F}, {1.0F, 1.0F, 1.0F});
    CHECK_VEC3_NEAR(TransformPoint(xThenY, Vec3{0.0F, 0.0F, 1.0F}), (Vec3{0.0F, -1.0F, 0.0F}), 1e-6);
    // Y then Z on +Z: Y(90) sends +Z to +X, Z(90) sends +X to +Y.
    const Matrix4 yThenZ = ComposeEulerXYZ({}, {0.0F, HalfPi, HalfPi}, {1.0F, 1.0F, 1.0F});
    CHECK_VEC3_NEAR(TransformPoint(yThenZ, Vec3{0.0F, 0.0F, 1.0F}), (Vec3{0.0F, 1.0F, 0.0F}), 1e-6);
    // Translation is applied last, unaffected by scale.
    const Matrix4 scaled = ComposeEulerXYZ({1.0F, 2.0F, 3.0F}, {}, {10.0F, 10.0F, 10.0F});
    CHECK_VEC3_EQ(TransformPoint(scaled, {}), 1.0F, 2.0F, 3.0F);
    CHECK_VEC3_EQ(TransformPoint(scaled, Vec3{1.0F, 0.0F, 0.0F}), 11.0F, 2.0F, 3.0F);
}

TEST_CASE("quaternion rotation matrices agree with Euler rotations") {
    const std::array<float, 7> angles{0.0F, 0.4F, -1.2F, HalfPi, 2.9F, -Pi, 3.0F};
    const std::array<Vec3, 3> axes{Vec3{1.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}, Vec3{0.0F, 0.0F, 1.0F}};
    for (const float a : angles) {
        CAPTURE(a);
        const std::array<Matrix4, 3> expected{MakeRotationX(a), MakeRotationY(a), MakeRotationZ(a)};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const Matrix4 fromQuaternion = MakeRotation(MakeQuaternionFromAxisAngle(axes[axis], a));
            for (std::size_t i = 0; i < 16; ++i) {
                CAPTURE(axis);
                CAPTURE(i);
                CHECK(fromQuaternion.values[i] == doctest::Approx(expected[axis].values[i]).epsilon(1e-6));
            }
        }
    }
    // Positive angle about +Y turns +Z toward +X (header contract).
    CHECK_VEC3_NEAR(Rotate(MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, HalfPi), Vec3{0.0F, 0.0F, 1.0F}),
                    (Vec3{1.0F, 0.0F, 0.0F}), 1e-6);

    // Rotate(q, v) == MakeRotation(q) applied to v for unit q.
    Lcg random{606U};
    for (int i = 0; i < 300; ++i) {
        const Vec3 axis = Normalize(random.RangeVec3(-1.0F, 1.0F) + Vec3{0.0F, 0.0F, 0.01F});
        const Quaternion q = MakeQuaternionFromAxisAngle(axis, random.Range(-Pi, Pi));
        const Vec3 v = random.RangeVec3(-5.0F, 5.0F);
        CHECK_VEC3_NEAR(Rotate(q, v), TransformPoint(MakeRotation(q), v), 1e-5);
        // Rotation preserves length.
        CHECK(Length(Rotate(q, v)) == doctest::Approx(Length(v)).epsilon(1e-5));
    }
}

TEST_CASE("ComposeTRS applies scale, rotation, then translation") {
    Lcg random{1001U};
    for (int i = 0; i < 300; ++i) {
        const Vec3 translation = random.RangeVec3(-10.0F, 10.0F);
        const Vec3 axis = Normalize(random.RangeVec3(-1.0F, 1.0F) + Vec3{0.01F, 0.0F, 0.0F});
        const Quaternion q = MakeQuaternionFromAxisAngle(axis, random.Range(-Pi, Pi));
        const Vec3 scale = random.RangeVec3(-3.0F, 3.0F);
        const Vec3 p = random.RangeVec3(-4.0F, 4.0F);
        const Matrix4 m = ComposeTRS(translation, q, scale);
        CHECK_VEC3_NEAR(TransformPoint(m, p), translation + Rotate(q, ScaleByHand(p, scale)), 1e-5);
        // Matches composing the elementary matrices T * R * S.
        const Matrix4 composed = Multiply(MakeTranslation(translation), Multiply(MakeRotation(q), MakeScale(scale)));
        for (std::size_t k = 0; k < 16; ++k) {
            CHECK(m.values[k] == doctest::Approx(composed.values[k]).epsilon(1e-5));
        }
        // The quaternion need not be unit length.
        const Quaternion scaledQ{q.x * 3.0F, q.y * 3.0F, q.z * 3.0F, q.w * 3.0F};
        const Matrix4 fromScaled = ComposeTRS(translation, scaledQ, scale);
        for (std::size_t k = 0; k < 16; ++k) {
            CHECK(fromScaled.values[k] == doctest::Approx(m.values[k]).epsilon(1e-5));
        }
    }
    // Identity quaternion and unit scale give a pure translation, bit exact.
    CHECK(SameValues(ComposeTRS({1.0F, 2.0F, 3.0F}, Quaternion{}, {1.0F, 1.0F, 1.0F}),
                     MakeTranslation({1.0F, 2.0F, 3.0F})));
    // Bottom row is (0, 0, 0, 1).
    const Matrix4 trs = ComposeTRS({1.0F, 2.0F, 3.0F}, MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, 0.5F),
                                   {2.0F, 2.0F, 2.0F});
    CHECK(trs.values[3] == 0.0F);
    CHECK(trs.values[7] == 0.0F);
    CHECK(trs.values[11] == 0.0F);
    CHECK(trs.values[15] == 1.0F);
}

TEST_CASE("ComposeTRS and MakeRotation tolerate a zero quaternion") {
    // A zero quaternion carries no rotation: the result must be finite and
    // apply no rotation (only scale and translation), never NaN.
    const Quaternion zero{0.0F, 0.0F, 0.0F, 0.0F};
    const Matrix4 m = ComposeTRS({1.0F, 2.0F, 3.0F}, zero, {2.0F, 3.0F, 4.0F});
    CHECK(IsFinite(m));
    CHECK(SameValues(m, Multiply(MakeTranslation({1.0F, 2.0F, 3.0F}), MakeScale({2.0F, 3.0F, 4.0F}))));
    const Matrix4 rotation = MakeRotation(zero);
    CHECK(IsFinite(rotation));
    CHECK(SameValues(rotation, Matrix4{}));
}

TEST_CASE("MakePerspective maps near to depth 0 and far to depth 1") {
    const float nearClip = 0.5F;
    const float farClip = 100.0F;
    // fov 90 degrees, aspect 2: yScale = 1 / tan(45) = 1, xScale = 0.5.
    const Matrix4 projection = MakePerspective(HalfPi, 2.0F, nearClip, farClip);

    const Vec4 atNear = Apply(projection, Vec4{1.0F, 0.5F, nearClip, 1.0F});
    CHECK(atNear.w == nearClip);
    CHECK(atNear.x == doctest::Approx(0.5F).epsilon(1e-6));
    CHECK(atNear.y == doctest::Approx(0.5F).epsilon(1e-6));
    CHECK(atNear.z / atNear.w == doctest::Approx(0.0F).epsilon(1e-6));

    const Vec4 atFar = Apply(projection, Vec4{0.0F, 0.0F, farClip, 1.0F});
    CHECK(atFar.w == farClip);
    CHECK(atFar.z / atFar.w == doctest::Approx(1.0F).epsilon(1e-6));

    // Frustum edges: at view depth z the visible half-height is z * tan(fov/2)
    // = z and the half-width is aspect * z = 2z; both map to NDC 1.
    const float depth = 10.0F;
    const Vec4 corner = Apply(projection, Vec4{2.0F * depth, depth, depth, 1.0F});
    CHECK(corner.w == depth);
    CHECK(corner.x / corner.w == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK(corner.y / corner.w == doctest::Approx(1.0F).epsilon(1e-6));
    const Vec4 lowerLeft = Apply(projection, Vec4{-2.0F * depth, -depth, depth, 1.0F});
    CHECK(lowerLeft.x / lowerLeft.w == doctest::Approx(-1.0F).epsilon(1e-6));
    CHECK(lowerLeft.y / lowerLeft.w == doctest::Approx(-1.0F).epsilon(1e-6));

    // Depth grows monotonically from near to far and stays in [0, 1].
    float previous = -1.0F;
    for (int i = 0; i <= 50; ++i) {
        const float z = nearClip + (farClip - nearClip) * static_cast<float>(i) / 50.0F;
        const Vec4 clip = Apply(projection, Vec4{0.0F, 0.0F, z, 1.0F});
        const float ndcDepth = clip.z / clip.w;
        CAPTURE(z);
        CHECK(ndcDepth > previous);
        CHECK(ndcDepth >= -1.0e-6F);
        CHECK(ndcDepth <= 1.0F + 1.0e-6F);
        previous = ndcDepth;
    }

    // fov 60 degrees, aspect 16:9: yScale = sqrt(3), xScale = sqrt(3) * 9 / 16.
    const Matrix4 wide = MakePerspective(DegreesToRadians(60.0F), 16.0F / 9.0F, 0.1F, 1000.0F);
    const Vec4 unitX = Apply(wide, Vec4{1.0F, 0.0F, 1.0F, 1.0F});
    const Vec4 unitY = Apply(wide, Vec4{0.0F, 1.0F, 1.0F, 1.0F});
    CHECK(unitX.x == doctest::Approx(1.7320508F * 9.0F / 16.0F).epsilon(1e-5));
    CHECK(unitY.y == doctest::Approx(1.7320508F).epsilon(1e-5));
    CHECK(unitX.w == 1.0F);
}

TEST_CASE("MakeOrthographicPixels maps pixels to clip space") {
    const float width = 1280.0F;
    const float height = 720.0F;
    const Matrix4 projection = MakeOrthographicPixels(width, height);
    const Vec4 topLeft = Apply(projection, Vec4{0.0F, 0.0F, 0.25F, 1.0F});
    CHECK(topLeft.x == -1.0F);
    CHECK(topLeft.y == 1.0F);
    CHECK(topLeft.z == 0.25F);
    CHECK(topLeft.w == 1.0F);
    const Vec4 bottomRight = Apply(projection, Vec4{width, height, 0.75F, 1.0F});
    CHECK(bottomRight.x == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK(bottomRight.y == doctest::Approx(-1.0F).epsilon(1e-6));
    CHECK(bottomRight.z == 0.75F);
    CHECK(bottomRight.w == 1.0F);
    const Vec4 center = Apply(projection, Vec4{width * 0.5F, height * 0.5F, 0.0F, 1.0F});
    CHECK(center.x == doctest::Approx(0.0F).epsilon(1e-6));
    CHECK(center.y == doctest::Approx(0.0F).epsilon(1e-6));
    // TransformPoint agrees because the bottom row is (0, 0, 0, 1).
    CHECK_VEC3_NEAR(TransformPoint(projection, Vec3{width, 0.0F, 0.5F}), (Vec3{1.0F, 1.0F, 0.5F}), 1e-6);
}

// ---------------------------------------------------------------------------
// linear/Matrix4.hpp, Matrix3.hpp: Transpose, Determinant, Inverse
// ---------------------------------------------------------------------------

TEST_CASE("Matrix4 Transpose") {
    Matrix4 m = Zero();
    for (std::size_t i = 0; i < 16; ++i) m.values[i] = static_cast<float>(i);
    const Matrix4 t = Transpose(m);
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            CHECK(t.values[column * 4 + row] == m.values[row * 4 + column]);
        }
    }
    CHECK(SameValues(Transpose(t), m));
    CHECK(SameValues(Transpose(Matrix4{}), Matrix4{}));
    // The transpose of a rotation is its inverse.
    const Matrix4 rotation = ComposeEulerXYZ({}, {0.3F, 0.4F, 0.5F}, {1.0F, 1.0F, 1.0F});
    CHECK(IsIdentity(Multiply(Transpose(rotation), rotation), 1e-6F));
}

TEST_CASE("Matrix4 Determinant on known matrices") {
    CHECK(Determinant(Matrix4{}) == 1.0F);
    CHECK(Determinant(Zero()) == 0.0F);
    CHECK(Determinant(MakeScale({2.0F, 3.0F, 4.0F})) == 24.0F);
    CHECK(Determinant(MakeScale({-1.0F, 1.0F, 1.0F})) == -1.0F);
    CHECK(Determinant(MakeTranslation({7.0F, 8.0F, 9.0F})) == 1.0F);
    // Hand-expanded: rows (2,0,1,3)(1,1,0,2)(0,3,1,1)(4,0,2,1) -> -25.
    const Matrix4 general = FromRows({{{2, 0, 1, 3}, {1, 1, 0, 2}, {0, 3, 1, 1}, {4, 0, 2, 1}}});
    CHECK(Determinant(general) == doctest::Approx(-25.0F).epsilon(1e-6));
    CHECK(Determinant(Transpose(general)) == doctest::Approx(-25.0F).epsilon(1e-6));
    // Swapping two rows negates; diag(1,2,3,4) has determinant 24.
    CHECK(Determinant(FromRows({{{0, 1, 0, 0}, {1, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}})) == -1.0F);
    CHECK(Determinant(FromRows({{{1, 9, 9, 9}, {0, 2, 9, 9}, {0, 0, 3, 9}, {0, 0, 0, 4}}})) == 24.0F);
    // det(TRS) = sx * sy * sz.
    const Matrix4 trs = ComposeTRS({5.0F, -1.0F, 2.0F}, MakeQuaternionFromAxisAngle({0.0F, 0.6F, 0.8F}, 1.3F),
                                   {2.0F, -0.5F, 3.0F});
    CHECK(Determinant(trs) == doctest::Approx(-3.0F).epsilon(1e-5));
    // det(AB) = det(A) det(B).
    CHECK(Determinant(Multiply(general, trs)) == doctest::Approx(75.0F).epsilon(1e-5));
}

TEST_CASE("Matrix4 Inverse") {
    const Matrix4 general = FromRows({{{2, 0, 1, 3}, {1, 1, 0, 2}, {0, 3, 1, 1}, {4, 0, 2, 1}}});
    const std::optional<Matrix4> inverse = Inverse(general);
    REQUIRE(inverse.has_value());
    // Exact inverse (adjugate / -25), rows in reading order.
    const Matrix4 expected = FromRows({{{-11.0F / 25.0F, 3.0F / 5.0F, -1.0F / 5.0F, 8.0F / 25.0F},
                                        {-9.0F / 25.0F, 2.0F / 5.0F, 1.0F / 5.0F, 2.0F / 25.0F},
                                        {17.0F / 25.0F, -6.0F / 5.0F, 2.0F / 5.0F, -1.0F / 25.0F},
                                        {2.0F / 5.0F, 0.0F, 0.0F, -1.0F / 5.0F}}});
    for (std::size_t i = 0; i < 16; ++i) {
        CAPTURE(i);
        CHECK(inverse->values[i] == doctest::Approx(expected.values[i]).epsilon(1e-6));
    }
    CHECK(IsIdentity(Multiply(general, *inverse), 1e-5F));
    CHECK(IsIdentity(Multiply(*inverse, general), 1e-5F));

    // Simple closed forms.
    const std::optional<Matrix4> translationInverse = Inverse(MakeTranslation({1.0F, -2.0F, 4.0F}));
    REQUIRE(translationInverse.has_value());
    CHECK_VEC3_EQ(TransformPoint(*translationInverse, {}), -1.0F, 2.0F, -4.0F);
    const std::optional<Matrix4> scaleInverse = Inverse(MakeScale({2.0F, 4.0F, -8.0F}));
    REQUIRE(scaleInverse.has_value());
    CHECK(scaleInverse->values[0] == 0.5F);
    CHECK(scaleInverse->values[5] == 0.25F);
    CHECK(scaleInverse->values[10] == -0.125F);
    REQUIRE(Inverse(Matrix4{}).has_value());
    CHECK(SameValues(*Inverse(Matrix4{}), Matrix4{}));

    // TRS matrices over a grid.
    Lcg random{4040U};
    for (int i = 0; i < 200; ++i) {
        const Vec3 axis = Normalize(random.RangeVec3(-1.0F, 1.0F) + Vec3{0.0F, 0.01F, 0.0F});
        const Quaternion q = MakeQuaternionFromAxisAngle(axis, random.Range(-Pi, Pi));
        const Vec3 scale{random.Range(0.25F, 4.0F), random.Range(0.25F, 4.0F), random.Range(0.25F, 4.0F)};
        const Matrix4 m = ComposeTRS(random.RangeVec3(-20.0F, 20.0F), q, scale);
        const std::optional<Matrix4> mInverse = Inverse(m);
        REQUIRE(mInverse.has_value());
        CHECK(IsIdentity(Multiply(m, *mInverse), 1e-4F));
        CHECK(IsIdentity(Multiply(*mInverse, m), 1e-4F));
    }

    // Singular and non-finite input.
    CHECK_FALSE(Inverse(Zero()).has_value());
    CHECK_FALSE(Inverse(Matrix4{{}}).has_value());
    CHECK_FALSE(Inverse(MakeScale({1.0F, 0.0F, 1.0F})).has_value());
    CHECK_FALSE(Inverse(FromRows({{{1, 2, 3, 4}, {2, 4, 6, 8}, {0, 1, 0, 1}, {1, 0, 1, 0}}})).has_value());
    Matrix4 withNaN;
    withNaN.values[5] = kNaN;
    CHECK_FALSE(Inverse(withNaN).has_value());
    Matrix4 withInf;
    withInf.values[0] = kInf;
    CHECK_FALSE(Inverse(withInf).has_value());
}

TEST_CASE("Matrix3 multiply, transform, transpose, determinant and inverse") {
    // Storage: values[column * 3 + row]; column c is the image of e_c.
    Matrix3 m;
    for (std::size_t i = 0; i < 9; ++i) m.values[i] = static_cast<float>(i + 1);
    CHECK_VEC3_EQ(Transform(m, Vec3{1.0F, 0.0F, 0.0F}), 1.0F, 2.0F, 3.0F);
    CHECK_VEC3_EQ(Transform(m, Vec3{0.0F, 1.0F, 0.0F}), 4.0F, 5.0F, 6.0F);
    CHECK_VEC3_EQ(Transform(m, Vec3{0.0F, 0.0F, 1.0F}), 7.0F, 8.0F, 9.0F);
    CHECK_VEC3_EQ(Transform(Matrix3{}, Vec3{3.0F, -2.0F, 1.0F}), 3.0F, -2.0F, 1.0F);

    const Matrix3 t = Transpose(m);
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t row = 0; row < 3; ++row) {
            CHECK(t.values[column * 3 + row] == m.values[row * 3 + column]);
        }
    }
    CHECK(Transpose(t).values == m.values);

    // Multiply(a, b) applies b first.
    const Matrix3 rotation = ToMatrix3(MakeRotationZ(HalfPi));
    const Matrix3 scale = ToMatrix3(MakeScale({2.0F, 1.0F, 1.0F}));
    // Scale first: (1,0,0) -> (2,0,0) -> rotate -> (0,2,0).
    CHECK_VEC3_NEAR(Transform(Multiply(rotation, scale), Vec3{1.0F, 0.0F, 0.0F}), (Vec3{0.0F, 2.0F, 0.0F}), 1e-6);
    // Rotate first: (1,0,0) -> (0,1,0) -> scale x only -> (0,1,0).
    CHECK_VEC3_NEAR(Transform(Multiply(scale, rotation), Vec3{1.0F, 0.0F, 0.0F}), (Vec3{0.0F, 1.0F, 0.0F}), 1e-6);
    CHECK(Multiply(Matrix3{}, m).values == m.values);
    CHECK(Multiply(m, Matrix3{}).values == m.values);
    Lcg random{90210U};
    for (int i = 0; i < 100; ++i) {
        Matrix3 a;
        Matrix3 b;
        for (std::size_t k = 0; k < 9; ++k) {
            a.values[k] = random.Range(-2.0F, 2.0F);
            b.values[k] = random.Range(-2.0F, 2.0F);
        }
        const Vec3 v = random.RangeVec3(-2.0F, 2.0F);
        CHECK_VEC3_NEAR(Transform(Multiply(a, b), v), Transform(a, Transform(b, v)), 1e-5);
    }

    // Hand-expanded: rows (2,1,0)(0,1,3)(1,0,1) -> det 5.
    const Matrix3 general = FromRows3({{{2, 1, 0}, {0, 1, 3}, {1, 0, 1}}});
    CHECK(Determinant(general) == 5.0F);
    CHECK(Determinant(Transpose(general)) == 5.0F);
    CHECK(Determinant(Matrix3{}) == 1.0F);
    CHECK(Determinant(FromRows3({{{0, 1, 0}, {1, 0, 0}, {0, 0, 1}}})) == -1.0F);
    CHECK(Determinant(m) == 0.0F);

    const std::optional<Matrix3> inverse = Inverse(general);
    REQUIRE(inverse.has_value());
    const Matrix3 expected = FromRows3({{{1.0F / 5.0F, -1.0F / 5.0F, 3.0F / 5.0F},
                                         {3.0F / 5.0F, 2.0F / 5.0F, -6.0F / 5.0F},
                                         {-1.0F / 5.0F, 1.0F / 5.0F, 2.0F / 5.0F}}});
    for (std::size_t i = 0; i < 9; ++i) {
        CAPTURE(i);
        CHECK(inverse->values[i] == doctest::Approx(expected.values[i]).epsilon(1e-6));
    }
    CHECK(IsIdentity(Multiply(general, *inverse), 1e-6F));
    CHECK(IsIdentity(Multiply(*inverse, general), 1e-6F));

    for (int i = 0; i < 100; ++i) {
        const Vec3 axis = Normalize(random.RangeVec3(-1.0F, 1.0F) + Vec3{0.0F, 0.0F, 0.01F});
        const Vec3 s{random.Range(0.25F, 4.0F), random.Range(0.25F, 4.0F), random.Range(0.25F, 4.0F)};
        const float angle = random.Range(-Pi, Pi);
        const Matrix3 rs = ToMatrix3(ComposeTRS({}, MakeQuaternionFromAxisAngle(axis, angle), s));
        const std::optional<Matrix3> rsInverse = Inverse(rs);
        REQUIRE(rsInverse.has_value());
        CHECK(IsIdentity(Multiply(rs, *rsInverse), 1e-5F));
    }

    CHECK_FALSE(Inverse(m).has_value());
    CHECK_FALSE(Inverse(Matrix3{{}}).has_value());
    Matrix3 withNaN;
    withNaN.values[4] = kNaN;
    CHECK_FALSE(Inverse(withNaN).has_value());

    CHECK(IsFinite(m));
    CHECK_FALSE(IsFinite(withNaN));
    Matrix3 withInf;
    withInf.values[8] = -kInf;
    CHECK_FALSE(IsFinite(withInf));
}

TEST_CASE("ToMatrix3 and ToMatrix4 round trip") {
    Matrix3 m3;
    for (std::size_t i = 0; i < 9; ++i) m3.values[i] = static_cast<float>(i) - 4.0F;
    CHECK(ToMatrix3(ToMatrix4(m3)).values == m3.values);

    const Matrix4 embedded = ToMatrix4(m3);
    // No translation and a (0, 0, 0, 1) last row/column.
    CHECK(embedded.values[3] == 0.0F);
    CHECK(embedded.values[7] == 0.0F);
    CHECK(embedded.values[11] == 0.0F);
    CHECK(embedded.values[12] == 0.0F);
    CHECK(embedded.values[13] == 0.0F);
    CHECK(embedded.values[14] == 0.0F);
    CHECK(embedded.values[15] == 1.0F);
    const Vec3 v{1.0F, 2.0F, 3.0F};
    const Vec3 viaMatrix3 = Transform(m3, v);
    const Vec3 viaMatrix4 = TransformVector(embedded, v);
    CHECK(viaMatrix3.x == viaMatrix4.x);
    CHECK(viaMatrix3.y == viaMatrix4.y);
    CHECK(viaMatrix3.z == viaMatrix4.z);

    // ToMatrix3 drops translation; rotation-only matrices survive exactly.
    const Matrix4 rotation = MakeRotationY(0.7F);
    CHECK(SameValues(ToMatrix4(ToMatrix3(rotation)), rotation));
    const Matrix4 withTranslation = Multiply(MakeTranslation({5.0F, 6.0F, 7.0F}), rotation);
    CHECK(SameValues(ToMatrix4(ToMatrix3(withTranslation)), rotation));
    CHECK(ToMatrix3(Matrix4{}).values == Matrix3{}.values);
}

TEST_CASE("Matrix4 IsFinite") {
    CHECK(IsFinite(Matrix4{}));
    CHECK(IsFinite(Zero()));
    Matrix4 m;
    m.values[13] = kInf;
    CHECK_FALSE(IsFinite(m));
    m.values[13] = kNaN;
    CHECK_FALSE(IsFinite(m));
}

// ---------------------------------------------------------------------------
// linear/Quaternion.hpp
// ---------------------------------------------------------------------------

TEST_CASE("Quaternion default, Dot, Conjugate and Inverse") {
    const Quaternion identity{};
    CHECK(identity.x == 0.0F);
    CHECK(identity.y == 0.0F);
    CHECK(identity.z == 0.0F);
    CHECK(identity.w == 1.0F);
    CHECK_VEC3_EQ(Rotate(identity, Vec3{1.0F, 2.0F, 3.0F}), 1.0F, 2.0F, 3.0F);

    const Quaternion a{1.0F, 2.0F, 3.0F, 4.0F};
    const Quaternion b{-1.0F, 0.5F, 2.0F, -2.0F};
    CHECK(Dot(a, b) == -2.0F);
    CHECK(Dot(a, a) == 30.0F);
    CHECK(LengthSquared(a) == 30.0F);

    const Quaternion conjugate = Conjugate(a);
    CHECK(conjugate.x == -1.0F);
    CHECK(conjugate.y == -2.0F);
    CHECK(conjugate.z == -3.0F);
    CHECK(conjugate.w == 4.0F);

    // q * q^-1 = identity for any non-zero q.
    CHECK_QUAT_NEAR(Multiply(a, Inverse(a)), Quaternion{}, 1e-6);
    CHECK_QUAT_NEAR(Multiply(Inverse(a), a), Quaternion{}, 1e-6);
    const Quaternion inverse = Inverse(a);
    CHECK(inverse.x == doctest::Approx(-1.0F / 30.0F).epsilon(1e-6));
    CHECK(inverse.w == doctest::Approx(4.0F / 30.0F).epsilon(1e-6));
    // For unit quaternions the inverse is the conjugate and undoes the rotation.
    const Quaternion unit = MakeQuaternionFromAxisAngle({0.0F, 0.6F, 0.8F}, 1.1F);
    CHECK_QUAT_NEAR(Inverse(unit), Conjugate(unit), 1e-6);
    const Vec3 v{0.3F, -2.0F, 5.0F};
    CHECK_VEC3_NEAR(Rotate(Conjugate(unit), Rotate(unit, v)), v, 1e-5);

    CHECK(IsFinite(a));
    CHECK_FALSE(IsFinite(Quaternion{kNaN, 0.0F, 0.0F, 1.0F}));
    CHECK_FALSE(IsFinite(Quaternion{0.0F, 0.0F, 0.0F, kInf}));
}

TEST_CASE("Quaternion Multiply is the Hamilton product and applies b first") {
    const Quaternion i{1.0F, 0.0F, 0.0F, 0.0F};
    const Quaternion j{0.0F, 1.0F, 0.0F, 0.0F};
    const Quaternion k{0.0F, 0.0F, 1.0F, 0.0F};
    // i j = k, j k = i, k i = j, j i = -k, i i = -1.
    CHECK_QUAT_EQ(Multiply(i, j), k);
    CHECK_QUAT_EQ(Multiply(j, k), i);
    CHECK_QUAT_EQ(Multiply(k, i), j);
    CHECK_QUAT_EQ(Multiply(j, i), (Quaternion{0.0F, 0.0F, -1.0F, 0.0F}));
    CHECK_QUAT_EQ(Multiply(i, i), (Quaternion{0.0F, 0.0F, 0.0F, -1.0F}));
    const Quaternion q{0.5F, -1.0F, 2.0F, 3.0F};
    CHECK_QUAT_EQ(Multiply(Quaternion{}, q), q);
    CHECK_QUAT_EQ(Multiply(q, Quaternion{}), q);

    // qY(90) * qX(90) applies X first: +Z -> -Y, which yaw leaves alone.
    const Quaternion yaw = MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, HalfPi);
    const Quaternion pitch = MakeQuaternionFromAxisAngle({1.0F, 0.0F, 0.0F}, HalfPi);
    CHECK_VEC3_NEAR(Rotate(Multiply(yaw, pitch), Vec3{0.0F, 0.0F, 1.0F}), (Vec3{0.0F, -1.0F, 0.0F}), 1e-6);
    // The other order: yaw first sends +Z to +X, which pitch leaves alone.
    CHECK_VEC3_NEAR(Rotate(Multiply(pitch, yaw), Vec3{0.0F, 0.0F, 1.0F}), (Vec3{1.0F, 0.0F, 0.0F}), 1e-6);

    Lcg random{777777U};
    for (int n = 0; n < 300; ++n) {
        const Quaternion a = RandomRotation(random, {0.01F, 0.0F, 0.0F});
        const Quaternion b = RandomRotation(random, {0.0F, 0.01F, 0.0F});
        const Vec3 v = random.RangeVec3(-3.0F, 3.0F);
        CHECK_VEC3_NEAR(Rotate(Multiply(a, b), v), Rotate(a, Rotate(b, v)), 1e-5);
        // Same order as Matrix4 Multiply.
        const Matrix4 product = MakeRotation(Multiply(a, b));
        const Matrix4 expected = Multiply(MakeRotation(a), MakeRotation(b));
        for (std::size_t idx = 0; idx < 16; ++idx) {
            CHECK(product.values[idx] == doctest::Approx(expected.values[idx]).epsilon(1e-5));
        }
    }
}

TEST_CASE("Quaternion Normalize and NormalizeOrIdentity") {
    const Quaternion n = Normalize(Quaternion{0.0F, 3.0F, 0.0F, 4.0F});
    CHECK(n.x == 0.0F);
    CHECK(n.y == doctest::Approx(0.6F).epsilon(1e-6));
    CHECK(n.z == 0.0F);
    CHECK(n.w == doctest::Approx(0.8F).epsilon(1e-6));
    CHECK_QUAT_NEAR(NormalizeOrIdentity(Quaternion{0.0F, 3.0F, 0.0F, 4.0F}), n, 1e-6);
    // Degenerate inputs become the identity.
    CHECK_QUAT_EQ(NormalizeOrIdentity(Quaternion{0.0F, 0.0F, 0.0F, 0.0F}), Quaternion{});
    CHECK_QUAT_EQ(NormalizeOrIdentity(Quaternion{1.0e-13F, 0.0F, 0.0F, 0.0F}), Quaternion{});
    CHECK_QUAT_EQ(NormalizeOrIdentity(Quaternion{kDenormMin, kDenormMin, 0.0F, 0.0F}), Quaternion{});
    CHECK_QUAT_EQ(NormalizeOrIdentity(Quaternion{kFloatMin, 0.0F, 0.0F, -kFloatMin}), Quaternion{});
    // Documented: Normalize does not guard zero.
    CHECK_FALSE(IsFinite(Normalize(Quaternion{0.0F, 0.0F, 0.0F, 0.0F})));

    Lcg random{5150U};
    for (int i = 0; i < 500; ++i) {
        const float scale = std::pow(10.0F, random.Range(-5.0F, 15.0F));
        const Quaternion q{random.Range(-1.0F, 1.0F) * scale, random.Range(-1.0F, 1.0F) * scale,
                           random.Range(-1.0F, 1.0F) * scale, random.Range(-1.0F, 1.0F) * scale};
        CHECK(LengthSquared(Normalize(q)) == doctest::Approx(1.0F).epsilon(1e-5));
        CHECK(LengthSquared(NormalizeOrIdentity(q)) == doctest::Approx(1.0F).epsilon(1e-5));
    }
}

TEST_CASE("Quaternion Slerp") {
    const Quaternion start{};
    const Quaternion end = MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, HalfPi);
    CHECK_QUAT_NEAR(Slerp(start, end, 0.0F), start, 1e-6);
    CHECK_QUAT_NEAR(Slerp(start, end, 1.0F), end, 1e-6);
    CHECK_QUAT_NEAR(Slerp(start, end, 0.5F), MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, Pi / 4.0F), 1e-6);
    CHECK_QUAT_NEAR(Slerp(start, end, 0.25F), MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, Pi / 8.0F), 1e-6);
    // Constant angular speed: the rotated vector sweeps a * t.
    for (int i = 0; i <= 10; ++i) {
        const float t = static_cast<float>(i) / 10.0F;
        const Vec3 rotated = Rotate(Slerp(start, end, t), Vec3{0.0F, 0.0F, 1.0F});
        CAPTURE(t);
        CHECK_VEC3_NEAR(rotated, (Vec3{std::sin(HalfPi * t), 0.0F, std::cos(HalfPi * t)}), 1e-5);
    }

    // Shortest path: q and -q are the same rotation, so the result is the same.
    const Quaternion negatedEnd{-end.x, -end.y, -end.z, -end.w};
    CHECK_QUAT_NEAR(Slerp(start, negatedEnd, 0.3F), Slerp(start, end, 0.3F), 1e-6);
    // A 270 degree yaw is a -90 degree yaw; halfway is -45 degrees, not +135.
    const Quaternion longWay = MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, 1.5F * Pi);
    const Vec3 halfway = Rotate(Slerp(start, longWay, 0.5F), Vec3{0.0F, 0.0F, 1.0F});
    CHECK_VEC3_NEAR(halfway, (Vec3{-std::sqrt(0.5F), 0.0F, std::sqrt(0.5F)}), 1e-5);

    // Inputs are normalized first; degenerate inputs become identity.
    const Quaternion scaledEnd{end.x * 5.0F, end.y * 5.0F, end.z * 5.0F, end.w * 5.0F};
    CHECK_QUAT_NEAR(Slerp(Quaternion{0.0F, 0.0F, 0.0F, 2.0F}, scaledEnd, 0.5F), Slerp(start, end, 0.5F), 1e-6);
    CHECK_QUAT_NEAR(Slerp(Quaternion{0.0F, 0.0F, 0.0F, 0.0F}, end, 0.0F), start, 1e-6);
    // Nearly parallel inputs stay unit and between the endpoints.
    const Quaternion close = MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, 1.0e-3F);
    const Quaternion nearlyParallel = Slerp(start, close, 0.5F);
    CHECK(LengthSquared(nearlyParallel) == doctest::Approx(1.0F).epsilon(1e-6));
    CHECK_QUAT_NEAR(nearlyParallel, MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, 0.5e-3F), 1e-6);
    // Identical inputs give that rotation.
    CHECK_QUAT_NEAR(Slerp(end, end, 0.7F), end, 1e-6);

    Lcg random{1234567U};
    for (int i = 0; i < 300; ++i) {
        const Quaternion a = RandomRotation(random, {0.01F, 0.0F, 0.0F});
        const Quaternion b = RandomRotation(random, {0.0F, 0.0F, 0.01F});
        const float t = random.Range(0.0F, 1.0F);
        const Quaternion result = Slerp(a, b, t);
        CHECK(LengthSquared(result) == doctest::Approx(1.0F).epsilon(1e-5));
        CHECK(IsFinite(result));
        // Endpoints reproduce the inputs as rotations (sign may differ).
        CHECK(std::fabs(Dot(Slerp(a, b, 0.0F), a)) == doctest::Approx(1.0F).epsilon(1e-5));
        CHECK(std::fabs(Dot(Slerp(a, b, 1.0F), b)) == doctest::Approx(1.0F).epsilon(1e-5));
    }
}

// ---------------------------------------------------------------------------
// geometry/Rect.hpp
// ---------------------------------------------------------------------------

TEST_CASE("Rect Intersection, Contains, Overlaps and IsFinite") {
    const Rect a{0.0F, 0.0F, 10.0F, 10.0F};
    const Rect b{5.0F, -2.0F, 10.0F, 4.0F};
    const Rect overlap = Intersection(a, b);
    CHECK(overlap.x == 5.0F);
    CHECK(overlap.y == 0.0F);
    CHECK(overlap.width == 5.0F);
    CHECK(overlap.height == 2.0F);
    const Rect symmetric = Intersection(b, a);
    CHECK(symmetric.x == 5.0F);
    CHECK(symmetric.width == 5.0F);
    // A rectangle inside another intersects to itself.
    const Rect inner{2.0F, 3.0F, 1.0F, 1.0F};
    const Rect same = Intersection(a, inner);
    CHECK(same.x == 2.0F);
    CHECK(same.y == 3.0F);
    CHECK(same.width == 1.0F);
    CHECK(same.height == 1.0F);
    // Disjoint: zero size.
    const Rect disjoint = Intersection(a, Rect{20.0F, 30.0F, 5.0F, 5.0F});
    CHECK(disjoint.width == 0.0F);
    CHECK(disjoint.height == 0.0F);
    const Rect disjointX = Intersection(a, Rect{20.0F, 0.0F, 5.0F, 5.0F});
    CHECK(disjointX.width == 0.0F);
    // Touching along an edge: zero width, full shared height.
    const Rect touching = Intersection(a, Rect{10.0F, 0.0F, 5.0F, 10.0F});
    CHECK(touching.x == 10.0F);
    CHECK(touching.width == 0.0F);
    CHECK(touching.height == 10.0F);

    // Closed edges.
    CHECK(Contains(a, Vec2{5.0F, 5.0F}));
    CHECK(Contains(a, Vec2{0.0F, 0.0F}));
    CHECK(Contains(a, Vec2{10.0F, 10.0F}));
    CHECK(Contains(a, Vec2{10.0F, 0.0F}));
    CHECK_FALSE(Contains(a, Vec2{10.01F, 5.0F}));
    CHECK_FALSE(Contains(a, Vec2{5.0F, -0.01F}));
    CHECK_FALSE(Contains(a, Vec2{kNaN, 5.0F}));

    CHECK(Overlaps(a, b));
    CHECK(Overlaps(a, inner));
    CHECK(Overlaps(a, Rect{10.0F, 0.0F, 5.0F, 5.0F}));
    CHECK(Overlaps(a, Rect{10.0F, 10.0F, 5.0F, 5.0F}));
    CHECK(Overlaps(a, Rect{-5.0F, -5.0F, 5.0F, 5.0F}));
    CHECK_FALSE(Overlaps(a, Rect{10.5F, 0.0F, 5.0F, 5.0F}));
    CHECK_FALSE(Overlaps(a, Rect{0.0F, -6.0F, 5.0F, 5.0F}));

    CHECK(IsFinite(a));
    CHECK_FALSE(IsFinite(Rect{0.0F, 0.0F, kInf, 1.0F}));
    CHECK_FALSE(IsFinite(Rect{0.0F, kNaN, 1.0F, 1.0F}));
    const Rect empty{};
    CHECK(empty.width == 0.0F);
    CHECK(empty.height == 0.0F);
}

// ---------------------------------------------------------------------------
// geometry/Segment.hpp, geometry/Ray.hpp
// ---------------------------------------------------------------------------

TEST_CASE("Segment PointAt and ClosestPoint") {
    const Segment segment{{0.0F, 0.0F, 0.0F}, {4.0F, 0.0F, 0.0F}};
    CHECK_VEC3_EQ(PointAt(segment, 0.0F), 0.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(PointAt(segment, 1.0F), 4.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(PointAt(segment, 0.25F), 1.0F, 0.0F, 0.0F);
    // t is not clamped.
    CHECK_VEC3_EQ(PointAt(segment, -0.5F), -2.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(PointAt(segment, 2.0F), 8.0F, 0.0F, 0.0F);

    CHECK_VEC3_EQ(ClosestPoint(Vec3{1.0F, 3.0F, 0.0F}, segment), 1.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{3.0F, -2.0F, 5.0F}, segment), 3.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{6.0F, 1.0F, 0.0F}, segment), 4.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{-2.0F, 1.0F, 0.0F}, segment), 0.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{2.0F, 0.0F, 0.0F}, segment), 2.0F, 0.0F, 0.0F);
    // Diagonal segment: (0,0,0)-(2,2,0), point (2,0,0) projects to (1,1,0).
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{2.0F, 0.0F, 0.0F}, Segment{{0.0F, 0.0F, 0.0F}, {2.0F, 2.0F, 0.0F}}),
                    (Vec3{1.0F, 1.0F, 0.0F}), 1e-6);
    // Degenerate segment returns start.
    const Segment point{{1.0F, 2.0F, 3.0F}, {1.0F, 2.0F, 3.0F}};
    CHECK_VEC3_EQ(ClosestPoint(Vec3{9.0F, 9.0F, 9.0F}, point), 1.0F, 2.0F, 3.0F);

    // Property: the closest point is no farther than any sampled point.
    Lcg random{8080U};
    for (int i = 0; i < 200; ++i) {
        const Segment s{random.RangeVec3(-5.0F, 5.0F), random.RangeVec3(-5.0F, 5.0F)};
        const Vec3 p = random.RangeVec3(-8.0F, 8.0F);
        const float best = Distance(p, ClosestPoint(p, s));
        for (int k = 0; k <= 20; ++k) {
            const float t = static_cast<float>(k) / 20.0F;
            CHECK(best <= Distance(p, PointAt(s, t)) + 1.0e-4F);
        }
    }
}

TEST_CASE("Segmentd ClosestPoint") {
    const Segmentd segment{{0.0, 0.0, 0.0}, {4.0, 0.0, 0.0}};
    const Vec3d interior = ClosestPoint(Vec3d{1.0, 3.0, 0.0}, segment);
    CHECK(interior.x == 1.0);
    CHECK(interior.y == 0.0);
    const Vec3d beyond = ClosestPoint(Vec3d{9.0, 1.0, 1.0}, segment);
    CHECK(beyond.x == 4.0);
    const Vec3d before = ClosestPoint(Vec3d{-9.0, 1.0, 1.0}, segment);
    CHECK(before.x == 0.0);
    const Segmentd degenerate{{1.0, 2.0, 3.0}, {1.0, 2.0, 3.0}};
    const Vec3d start = ClosestPoint(Vec3d{0.0, 0.0, 0.0}, degenerate);
    CHECK(start.x == 1.0);
    CHECK(start.y == 2.0);
    CHECK(start.z == 3.0);
}

TEST_CASE("Ray PointAt measures t in units of direction") {
    const Ray ray{{1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, 2.0F}};
    CHECK_VEC3_EQ(PointAt(ray, 0.0F), 1.0F, 2.0F, 3.0F);
    CHECK_VEC3_EQ(PointAt(ray, 1.0F), 1.0F, 2.0F, 5.0F);
    CHECK_VEC3_EQ(PointAt(ray, 2.5F), 1.0F, 2.0F, 8.0F);
}

// ---------------------------------------------------------------------------
// geometry/Aabb.hpp
// ---------------------------------------------------------------------------

TEST_CASE("Aabb queries") {
    const Aabb box{{-1.0F, 0.0F, 2.0F}, {3.0F, 4.0F, 4.0F}};
    CHECK_VEC3_EQ(Center(box), 1.0F, 2.0F, 3.0F);
    CHECK_VEC3_EQ(Extents(box), 2.0F, 2.0F, 1.0F);

    CHECK(Contains(box, Vec3{0.0F, 1.0F, 3.0F}));
    CHECK(Contains(box, Vec3{-1.0F, 0.0F, 2.0F}));
    CHECK(Contains(box, Vec3{3.0F, 4.0F, 4.0F}));
    CHECK(Contains(box, Vec3{3.0F, 2.0F, 3.0F}));
    CHECK_FALSE(Contains(box, Vec3{3.001F, 2.0F, 3.0F}));
    CHECK_FALSE(Contains(box, Vec3{0.0F, -0.001F, 3.0F}));
    CHECK_FALSE(Contains(box, Vec3{0.0F, 1.0F, 5.0F}));
    CHECK_FALSE(Contains(box, Vec3{kNaN, 1.0F, 3.0F}));

    const Aabb other{{2.0F, -3.0F, 3.0F}, {5.0F, 1.0F, 3.5F}};
    const Aabb merged = Merge(box, other);
    CHECK_VEC3_EQ(merged.minimum, -1.0F, -3.0F, 2.0F);
    CHECK_VEC3_EQ(merged.maximum, 5.0F, 4.0F, 4.0F);
    const Aabb expanded = Expand(box, Vec3{10.0F, 1.0F, -5.0F});
    CHECK_VEC3_EQ(expanded.minimum, -1.0F, 0.0F, -5.0F);
    CHECK_VEC3_EQ(expanded.maximum, 10.0F, 4.0F, 4.0F);
    const Aabb unchanged = Expand(box, Vec3{0.0F, 1.0F, 3.0F});
    CHECK_VEC3_EQ(unchanged.minimum, -1.0F, 0.0F, 2.0F);
    CHECK_VEC3_EQ(unchanged.maximum, 3.0F, 4.0F, 4.0F);

    // Inside returns the point itself; outside clamps per axis.
    CHECK_VEC3_EQ(ClosestPoint(Vec3{0.5F, 1.5F, 2.5F}, box), 0.5F, 1.5F, 2.5F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{-5.0F, 2.0F, 3.0F}, box), -1.0F, 2.0F, 3.0F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{9.0F, 9.0F, -9.0F}, box), 3.0F, 4.0F, 2.0F);

    CHECK(Overlaps(box, other));
    CHECK(Overlaps(other, box));
    CHECK(Overlaps(box, Aabb{{3.0F, 4.0F, 4.0F}, {5.0F, 5.0F, 5.0F}}));
    CHECK(Overlaps(box, Aabb{{3.0F, 0.0F, 2.0F}, {6.0F, 1.0F, 3.0F}}));
    CHECK_FALSE(Overlaps(box, Aabb{{3.5F, 0.0F, 2.0F}, {6.0F, 1.0F, 3.0F}}));
    CHECK_FALSE(Overlaps(box, Aabb{{0.0F, 0.0F, 4.5F}, {1.0F, 1.0F, 5.0F}}));

    CHECK(IsFinite(box));
    CHECK_FALSE(IsFinite(Aabb{{-kInf, 0.0F, 0.0F}, {}}));
    CHECK_FALSE(IsFinite(Aabb{{}, {0.0F, kNaN, 0.0F}}));
}

TEST_CASE("Aabbd and ToAabbd") {
    const Aabb box{{-1.5F, 0.1F, 2.0F}, {3.0F, 4.0F, 1.0e19F}};
    const Aabbd wide = ToAabbd(box);
    CHECK(wide.minimum.x == -1.5);
    CHECK(wide.minimum.y == static_cast<double>(0.1F));
    CHECK(wide.maximum.z == static_cast<double>(1.0e19F));
    const Aabbd unit{{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}};
    const Vec3d inside = ClosestPoint(Vec3d{0.25, 0.5, 0.75}, unit);
    CHECK(inside.x == 0.25);
    CHECK(inside.y == 0.5);
    CHECK(inside.z == 0.75);
    const Vec3d outside = ClosestPoint(Vec3d{-2.0, 0.5, 9.0}, unit);
    CHECK(outside.x == 0.0);
    CHECK(outside.y == 0.5);
    CHECK(outside.z == 1.0);
}

// ---------------------------------------------------------------------------
// geometry/Plane.hpp
// ---------------------------------------------------------------------------

TEST_CASE("Plane construction, signed distance and closest point") {
    const Plane defaultPlane{};
    CHECK_VEC3_EQ(defaultPlane.normal, 0.0F, 1.0F, 0.0F);
    CHECK(defaultPlane.distance == 0.0F);
    CHECK(SignedDistance(defaultPlane, Vec3{5.0F, 2.0F, -1.0F}) == 2.0F);

    // MakePlane(point, normal) normalizes the normal.
    const Plane fromPoint = MakePlane(Vec3{7.0F, 2.0F, -3.0F}, Vec3{0.0F, 5.0F, 0.0F});
    CHECK_VEC3_EQ(fromPoint.normal, 0.0F, 1.0F, 0.0F);
    CHECK(fromPoint.distance == 2.0F);
    const Plane tilted = MakePlane(Vec3{0.0F, 0.0F, 0.0F}, Vec3{3.0F, 0.0F, 4.0F});
    CHECK_VEC3_NEAR(tilted.normal, (Vec3{0.6F, 0.0F, 0.8F}), 1e-6);
    CHECK(tilted.distance == 0.0F);
    const Plane offset = MakePlane(Vec3{1.0F, 1.0F, 1.0F}, Vec3{1.0F, 1.0F, 1.0F});
    CHECK(Length(offset.normal) == doctest::Approx(1.0F).epsilon(1e-6));
    // dot(n, p) = 3 / sqrt(3) = sqrt(3).
    CHECK(offset.distance == doctest::Approx(1.7320508F).epsilon(1e-6));

    // MakePlane(a, b, c): normal = Normalize(Cross(b - a, c - a)).
    // Cross((1,0,0), (0,0,1)) = (0,-1,0).
    const Plane down = MakePlane(Vec3{0.0F, 0.0F, 0.0F}, Vec3{1.0F, 0.0F, 0.0F}, Vec3{0.0F, 0.0F, 1.0F});
    CHECK_VEC3_EQ(down.normal, 0.0F, -1.0F, 0.0F);
    CHECK(down.distance == 0.0F);
    // Cross((0,0,1), (1,0,0)) = (0,1,0), plane y = 3.
    const Plane up = MakePlane(Vec3{0.0F, 3.0F, 0.0F}, Vec3{0.0F, 3.0F, 1.0F}, Vec3{1.0F, 3.0F, 0.0F});
    CHECK_VEC3_EQ(up.normal, 0.0F, 1.0F, 0.0F);
    CHECK(up.distance == 3.0F);
    // Non-unit edges still produce a unit normal: Cross((2,0,0),(0,4,0)) = (0,0,8).
    const Plane forward = MakePlane(Vec3{0.0F, 0.0F, 5.0F}, Vec3{2.0F, 0.0F, 5.0F}, Vec3{0.0F, 4.0F, 5.0F});
    CHECK_VEC3_EQ(forward.normal, 0.0F, 0.0F, 1.0F);
    CHECK(forward.distance == 5.0F);

    // Signed distance is positive on the side the normal points to.
    CHECK(SignedDistance(up, Vec3{0.0F, 5.0F, 0.0F}) == 2.0F);
    CHECK(SignedDistance(up, Vec3{4.0F, 1.0F, -7.0F}) == -2.0F);
    CHECK(SignedDistance(up, Vec3{9.0F, 3.0F, 9.0F}) == 0.0F);
    CHECK(SignedDistance(down, Vec3{0.0F, 5.0F, 0.0F}) == -5.0F);

    CHECK_VEC3_EQ(ClosestPoint(Vec3{4.0F, 10.0F, -2.0F}, up), 4.0F, 3.0F, -2.0F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{4.0F, -10.0F, -2.0F}, up), 4.0F, 3.0F, -2.0F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{4.0F, 3.0F, -2.0F}, up), 4.0F, 3.0F, -2.0F);
    Lcg random{1111U};
    for (int i = 0; i < 200; ++i) {
        const Vec3 point = random.RangeVec3(-5.0F, 5.0F);
        const Vec3 normal = random.RangeVec3(-1.0F, 1.0F) + Vec3{0.0F, 0.0F, 0.01F};
        const Plane plane = MakePlane(point, normal);
        const Vec3 p = random.RangeVec3(-10.0F, 10.0F);
        const Vec3 projected = ClosestPoint(p, plane);
        CHECK(SignedDistance(plane, projected) == doctest::Approx(0.0F).epsilon(1e-4));
        // The offset is along the normal.
        CHECK(Length(Cross(p - projected, plane.normal)) == doctest::Approx(0.0F).epsilon(1e-4));
    }
}

// ---------------------------------------------------------------------------
// geometry/Sphere.hpp, geometry/Capsule.hpp
// ---------------------------------------------------------------------------

TEST_CASE("Sphere containment and overlap") {
    const Sphere sphere{{1.0F, 2.0F, 3.0F}, 2.0F};
    CHECK(Contains(sphere, Vec3{1.0F, 2.0F, 3.0F}));
    CHECK(Contains(sphere, Vec3{3.0F, 2.0F, 3.0F}));
    CHECK(Contains(sphere, Vec3{1.0F, 0.0F, 3.0F}));
    CHECK_FALSE(Contains(sphere, Vec3{3.01F, 2.0F, 3.0F}));
    CHECK_FALSE(Contains(sphere, Vec3{2.5F, 3.5F, 3.0F}));

    CHECK(Overlaps(sphere, Sphere{{4.0F, 2.0F, 3.0F}, 1.5F}));
    // Touching: centres 3 apart, radii 2 + 1.
    CHECK(Overlaps(sphere, Sphere{{4.0F, 2.0F, 3.0F}, 1.0F}));
    CHECK_FALSE(Overlaps(sphere, Sphere{{4.0F, 2.0F, 3.0F}, 0.9F}));
    CHECK(Overlaps(sphere, Sphere{{1.0F, 2.0F, 3.0F}, 0.1F}));

    const Aabb box{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    CHECK(Overlaps(Sphere{{0.0F, 0.0F, 0.0F}, 0.5F}, box));
    CHECK(Overlaps(Sphere{{2.0F, 0.0F, 0.0F}, 1.0F}, box));
    CHECK(Overlaps(box, Sphere{{2.0F, 0.0F, 0.0F}, 1.0F}));
    CHECK_FALSE(Overlaps(Sphere{{2.0F, 0.0F, 0.0F}, 0.99F}, box));
    // Near a corner: distance to (1,1,1) from (2,2,2) is sqrt(3) ~ 1.732.
    CHECK(Overlaps(Sphere{{2.0F, 2.0F, 2.0F}, 1.75F}, box));
    CHECK_FALSE(Overlaps(Sphere{{2.0F, 2.0F, 2.0F}, 1.7F}, box));
    CHECK_FALSE(Overlaps(box, Sphere{{2.0F, 2.0F, 2.0F}, 1.7F}));
}

TEST_CASE("Capsule axis and containment") {
    const Capsule capsule{{0.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 0.0F}, 1.0F};
    const Segment axis = Axis(capsule);
    CHECK_VEC3_EQ(axis.start, 0.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(axis.end, 0.0F, 2.0F, 0.0F);

    // Cylinder part.
    CHECK(Contains(capsule, Vec3{0.0F, 1.0F, 0.0F}));
    CHECK(Contains(capsule, Vec3{1.0F, 1.0F, 0.0F}));
    CHECK(Contains(capsule, Vec3{0.0F, 0.5F, -1.0F}));
    CHECK_FALSE(Contains(capsule, Vec3{1.01F, 1.0F, 0.0F}));
    // Hemisphere caps: surface points are contained, the box corner is not.
    CHECK(Contains(capsule, Vec3{0.0F, 3.0F, 0.0F}));
    CHECK(Contains(capsule, Vec3{0.0F, -1.0F, 0.0F}));
    CHECK(Contains(capsule, Vec3{0.7F, 2.7F, 0.0F}));
    CHECK_FALSE(Contains(capsule, Vec3{0.8F, 2.8F, 0.0F}));
    CHECK_FALSE(Contains(capsule, Vec3{1.0F, 2.5F, 0.0F}));
    CHECK_FALSE(Contains(capsule, Vec3{0.0F, 3.01F, 0.0F}));
    CHECK_FALSE(Contains(capsule, Vec3{0.0F, -1.01F, 0.0F}));
    CHECK_FALSE(Contains(capsule, Vec3{-0.8F, -0.8F, 0.0F}));

    // Coincident endpoints form a sphere.
    const Capsule sphere{{1.0F, 1.0F, 1.0F}, {1.0F, 1.0F, 1.0F}, 0.5F};
    CHECK(Contains(sphere, Vec3{1.0F, 1.0F, 1.0F}));
    CHECK(Contains(sphere, Vec3{1.5F, 1.0F, 1.0F}));
    CHECK(Contains(sphere, Vec3{1.0F, 0.5F, 1.0F}));
    CHECK_FALSE(Contains(sphere, Vec3{1.0F, 1.0F, 1.51F}));
    CHECK_FALSE(Contains(sphere, Vec3{1.4F, 1.4F, 1.0F}));
}

// ---------------------------------------------------------------------------
// geometry/Triangle.hpp
// ---------------------------------------------------------------------------

TEST_CASE("Triangle normal, area and centroid") {
    const Triangle triangle{{0.0F, 0.0F, 0.0F}, {3.0F, 0.0F, 0.0F}, {0.0F, 4.0F, 0.0F}};
    // Cross((3,0,0), (0,4,0)) = (0,0,12).
    CHECK_VEC3_EQ(Normal(triangle), 0.0F, 0.0F, 12.0F);
    CHECK_VEC3_EQ(UnitNormal(triangle), 0.0F, 0.0F, 1.0F);
    CHECK(Area(triangle) == 6.0F);
    CHECK_VEC3_NEAR(Centroid(triangle), (Vec3{1.0F, 4.0F / 3.0F, 0.0F}), 1e-6);
    // Swapping the winding flips the normal; the area is unchanged.
    const Triangle flipped{triangle.a, triangle.c, triangle.b};
    CHECK_VEC3_EQ(Normal(flipped), 0.0F, 0.0F, -12.0F);
    CHECK_VEC3_EQ(UnitNormal(flipped), 0.0F, 0.0F, -1.0F);
    CHECK(Area(flipped) == 6.0F);
    // Cyclic rotation of the vertices keeps the normal.
    CHECK_VEC3_EQ(Normal(Triangle{triangle.b, triangle.c, triangle.a}), 0.0F, 0.0F, 12.0F);
    // Tilted triangle in the plane x + y + z = 1: area sqrt(3) / 2.
    const Triangle tilted{{1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}};
    CHECK(Area(tilted) == doctest::Approx(0.8660254F).epsilon(1e-6));
    CHECK_VEC3_NEAR(UnitNormal(tilted), (Vec3{0.57735027F, 0.57735027F, 0.57735027F}), 1e-6);
    CHECK_VEC3_NEAR(Centroid(tilted), (Vec3{1.0F / 3.0F, 1.0F / 3.0F, 1.0F / 3.0F}), 1e-6);
    // Degenerate triangles have zero area; UnitNormal is documented as NaN.
    const Triangle collinear{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {2.0F, 0.0F, 0.0F}};
    CHECK(Area(collinear) == 0.0F);
    CHECK_FALSE(IsFinite(UnitNormal(collinear)));
}

TEST_CASE("Triangle ClosestPoint covers all seven Voronoi regions") {
    const Triangle triangle{{0.0F, 0.0F, 0.0F}, {4.0F, 0.0F, 0.0F}, {0.0F, 4.0F, 0.0F}};
    // Vertex regions.
    CHECK_VEC3_EQ(ClosestPoint(Vec3{-1.0F, -1.0F, 2.0F}, triangle), 0.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{5.0F, -1.0F, 0.0F}, triangle), 4.0F, 0.0F, 0.0F);
    CHECK_VEC3_EQ(ClosestPoint(Vec3{-1.0F, 5.0F, 1.0F}, triangle), 0.0F, 4.0F, 0.0F);
    // Edge regions.
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{2.0F, -1.0F, 3.0F}, triangle), (Vec3{2.0F, 0.0F, 0.0F}), 1e-6);
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{-1.0F, 2.0F, 0.0F}, triangle), (Vec3{0.0F, 2.0F, 0.0F}), 1e-6);
    // Edge bc lies on x + y = 4; (3,3) projects to (2,2).
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{3.0F, 3.0F, -1.0F}, triangle), (Vec3{2.0F, 2.0F, 0.0F}), 1e-6);
    // Interior: drop the z offset.
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{1.0F, 1.0F, 5.0F}, triangle), (Vec3{1.0F, 1.0F, 0.0F}), 1e-6);
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{1.0F, 2.0F, -5.0F}, triangle), (Vec3{1.0F, 2.0F, 0.0F}), 1e-6);
    // Points on the triangle map to themselves.
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{2.0F, 2.0F, 0.0F}, triangle), (Vec3{2.0F, 2.0F, 0.0F}), 1e-6);
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{0.0F, 0.0F, 0.0F}, triangle), (Vec3{0.0F, 0.0F, 0.0F}), 1e-6);

    // Property: never farther than any barycentric sample and always on the triangle.
    Lcg random{3141U};
    for (int i = 0; i < 200; ++i) {
        const Triangle t{random.RangeVec3(-5.0F, 5.0F), random.RangeVec3(-5.0F, 5.0F), random.RangeVec3(-5.0F, 5.0F)};
        const Vec3 p = random.RangeVec3(-8.0F, 8.0F);
        const Vec3 closest = ClosestPoint(p, t);
        REQUIRE(IsFinite(closest));
        const float best = Distance(p, closest);
        for (int u = 0; u <= 10; ++u) {
            for (int v = 0; u + v <= 10; ++v) {
                const float fu = static_cast<float>(u) / 10.0F;
                const float fv = static_cast<float>(v) / 10.0F;
                const Vec3 sample = t.a + (t.b - t.a) * fu + (t.c - t.a) * fv;
                CHECK(best <= Distance(p, sample) + 1.0e-3F);
            }
        }
    }
}

TEST_CASE("Triangle ClosestPoint on degenerate triangles stays finite") {
    // Header contract: degenerate triangles fall back to the nearest edge or
    // vertex region. The closest point of a degenerate triangle is the
    // closest point of the segment it collapses to.
    const Vec3 origin{0.0F, 0.0F, 0.0F};
    const Vec3 four{4.0F, 0.0F, 0.0F};
    const Vec3 two{2.0F, 0.0F, 0.0F};
    // Distinct collinear vertices.
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{1.0F, 1.0F, 0.0F}, Triangle{origin, two, four}), (Vec3{1.0F, 0.0F, 0.0F}),
                    1e-6);
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{3.0F, 1.0F, 0.0F}, Triangle{origin, two, four}), (Vec3{3.0F, 0.0F, 0.0F}),
                    1e-6);
    // All vertices coincident.
    CHECK_VEC3_EQ(ClosestPoint(Vec3{3.0F, 1.0F, 0.0F}, Triangle{two, two, two}), 2.0F, 0.0F, 0.0F);
    // c == a and b == c.
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{2.0F, 1.0F, 0.0F}, Triangle{origin, four, origin}), two, 1e-6);
    CHECK_VEC3_NEAR(ClosestPoint(Vec3{2.0F, 1.0F, 0.0F}, Triangle{origin, four, four}), two, 1e-6);
    // a == b: the triangle is the segment a-c.
    const Vec3 collapsedAb = ClosestPoint(Vec3{2.0F, 1.0F, 0.0F}, Triangle{origin, origin, four});
    CHECK(IsFinite(collapsedAb));
    CHECK_VEC3_NEAR(collapsedAb, two, 1e-6);
    const Vec3 collapsedAbBeyond = ClosestPoint(Vec3{5.0F, 1.0F, 0.0F}, Triangle{origin, origin, four});
    CHECK(IsFinite(collapsedAbBeyond));
    CHECK_VEC3_NEAR(collapsedAbBeyond, four, 1e-6);

    // Grid sweep over every degenerate vertex arrangement; report non-finite results.
    const std::array<Triangle, 6> degenerate{
        Triangle{origin, origin, four}, Triangle{origin, four, origin}, Triangle{four, origin, origin},
        Triangle{origin, two, four},    Triangle{four, two, origin},    Triangle{two, two, two}};
    for (std::size_t index = 0; index < degenerate.size(); ++index) {
        int nonFinite = 0;
        for (int ix = -2; ix <= 6; ++ix) {
            for (int iy = -2; iy <= 2; ++iy) {
                const Vec3 p{static_cast<float>(ix), static_cast<float>(iy), 0.5F};
                if (!IsFinite(ClosestPoint(p, degenerate[index]))) ++nonFinite;
            }
        }
        CAPTURE(index);
        CHECK(nonFinite == 0);
    }

    // Non-integer coordinates leave rounding residue in the region tests (and
    // FMA contraction changes it), and nearly coincident vertices push the edge
    // denominators toward zero. Every vertex order must stay finite and match the
    // closest point of the segment the triangle collapses to.
    Lcg random{1735U};
    const std::array<float, 4> offsets{0.0F, 1e-7F, 1e-12F, 1e-30F};
    for (int i = 0; i < 400; ++i) {
        const Vec3 a = random.RangeVec3(-5.0F, 5.0F);
        const Vec3 c = random.RangeVec3(-5.0F, 5.0F);
        const Vec3 p = random.RangeVec3(-8.0F, 8.0F);
        for (const float offset : offsets) {
            const Vec3 b = a + Vec3{offset, 0.0F, 0.0F};
            const Vec3 expected = ClosestPoint(p, Segment{a, c});
            const std::array<Triangle, 3> orders{Triangle{a, b, c}, Triangle{a, c, b}, Triangle{c, a, b}};
            for (const Triangle& triangle : orders) {
                const Vec3 actual = ClosestPoint(p, triangle);
                CAPTURE(i);
                CAPTURE(offset);
                REQUIRE(IsFinite(actual));
                CHECK(Distance(actual, expected) <= 1e-3F * (1.0F + Length(p)));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// geometry/Intersection.hpp
// ---------------------------------------------------------------------------

TEST_CASE("Ray-Plane intersection") {
    const Plane plane{{0.0F, 1.0F, 0.0F}, 2.0F};
    const std::optional<float> hit = Intersect(Ray{{0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}}, plane);
    REQUIRE(hit.has_value());
    CHECK(*hit == 2.0F);
    // Doubling the direction halves t.
    const std::optional<float> doubled = Intersect(Ray{{0.0F, 0.0F, 0.0F}, {0.0F, 2.0F, 0.0F}}, plane);
    REQUIRE(doubled.has_value());
    CHECK(*doubled == 1.0F);
    // From the positive side going down.
    const std::optional<float> fromAbove = Intersect(Ray{{3.0F, 5.0F, 1.0F}, {0.0F, -1.0F, 0.0F}}, plane);
    REQUIRE(fromAbove.has_value());
    CHECK(*fromAbove == 3.0F);
    // Oblique: (1,1,0) direction reaches y = 2 at t = 2, point (2,2,0).
    const Ray oblique{{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 0.0F}};
    const std::optional<float> obliqueHit = Intersect(oblique, plane);
    REQUIRE(obliqueHit.has_value());
    CHECK(*obliqueHit == 2.0F);
    CHECK_VEC3_EQ(PointAt(oblique, *obliqueHit), 2.0F, 2.0F, 0.0F);
    // Origin on the plane.
    const std::optional<float> onPlane = Intersect(Ray{{0.0F, 2.0F, 0.0F}, {0.0F, 1.0F, 0.0F}}, plane);
    REQUIRE(onPlane.has_value());
    CHECK(*onPlane == 0.0F);
    // Behind and parallel.
    CHECK_FALSE(Intersect(Ray{{0.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}}, plane).has_value());
    CHECK_FALSE(Intersect(Ray{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, plane).has_value());
    CHECK_FALSE(Intersect(Ray{{0.0F, 2.0F, 0.0F}, {1.0F, 0.0F, 1.0F}}, plane).has_value());
}

TEST_CASE("Ray-Sphere intersection") {
    const Sphere sphere{{0.0F, 0.0F, 0.0F}, 1.0F};
    const std::optional<float> hit = Intersect(Ray{{0.0F, 0.0F, -5.0F}, {0.0F, 0.0F, 1.0F}}, sphere);
    REQUIRE(hit.has_value());
    CHECK(*hit == 4.0F);
    const std::optional<float> doubled = Intersect(Ray{{0.0F, 0.0F, -5.0F}, {0.0F, 0.0F, 2.0F}}, sphere);
    REQUIRE(doubled.has_value());
    CHECK(*doubled == 2.0F);
    // Off-centre: y = 0.6 enters where z = -0.8, so t = 5 - 0.8.
    const std::optional<float> offCentre = Intersect(Ray{{0.0F, 0.6F, -5.0F}, {0.0F, 0.0F, 1.0F}}, sphere);
    REQUIRE(offCentre.has_value());
    CHECK(*offCentre == doctest::Approx(4.2F).epsilon(1e-6));
    // Origin inside or on the surface returns 0.
    const std::optional<float> inside = Intersect(Ray{{0.2F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, sphere);
    REQUIRE(inside.has_value());
    CHECK(*inside == 0.0F);
    const std::optional<float> surface = Intersect(Ray{{0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}}, sphere);
    REQUIRE(surface.has_value());
    CHECK(*surface == 0.0F);
    // Tangent: grazing the top at (0,1,0) after t = 5.
    const std::optional<float> tangent = Intersect(Ray{{0.0F, 1.0F, -5.0F}, {0.0F, 0.0F, 1.0F}}, sphere);
    REQUIRE(tangent.has_value());
    CHECK(*tangent == 5.0F);
    // Miss and pointing away.
    CHECK_FALSE(Intersect(Ray{{0.0F, 2.0F, -5.0F}, {0.0F, 0.0F, 1.0F}}, sphere).has_value());
    CHECK_FALSE(Intersect(Ray{{0.0F, 0.0F, -5.0F}, {0.0F, 0.0F, -1.0F}}, sphere).has_value());
    CHECK_FALSE(Intersect(Ray{{0.0F, 0.0F, -5.0F}, {0.0F, 1.0F, 0.0F}}, sphere).has_value());
    // Translated sphere.
    const std::optional<float> moved = Intersect(Ray{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}},
                                                 Sphere{{10.0F, 0.0F, 0.0F}, 2.0F});
    REQUIRE(moved.has_value());
    CHECK(*moved == 8.0F);
}

TEST_CASE("Ray-Aabb intersection") {
    const Aabb box{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    // Hit along every axis from both sides.
    const std::array<Vec3, 3> axes{Vec3{1.0F, 0.0F, 0.0F}, Vec3{0.0F, 1.0F, 0.0F}, Vec3{0.0F, 0.0F, 1.0F}};
    for (const Vec3 axis : axes) {
        const std::optional<float> positive = Intersect(Ray{axis * -5.0F, axis}, box);
        REQUIRE(positive.has_value());
        CHECK(*positive == 4.0F);
        const std::optional<float> negative = Intersect(Ray{axis * 5.0F, -axis}, box);
        REQUIRE(negative.has_value());
        CHECK(*negative == 4.0F);
        const std::optional<float> doubled = Intersect(Ray{axis * -5.0F, axis * 2.0F}, box);
        REQUIRE(doubled.has_value());
        CHECK(*doubled == 2.0F);
        // Pointing away.
        CHECK_FALSE(Intersect(Ray{axis * 5.0F, axis}, box).has_value());
    }
    // Origin inside returns 0.
    const std::optional<float> inside = Intersect(Ray{{0.5F, 0.0F, -0.5F}, {0.3F, -1.0F, 2.0F}}, box);
    REQUIRE(inside.has_value());
    CHECK(*inside == 0.0F);
    // Axis-parallel ray: zero components inside the slab hit, outside miss.
    const std::optional<float> inSlab = Intersect(Ray{{-5.0F, 0.5F, -0.5F}, {1.0F, 0.0F, 0.0F}}, box);
    REQUIRE(inSlab.has_value());
    CHECK(*inSlab == 4.0F);
    CHECK_FALSE(Intersect(Ray{{-5.0F, 3.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, box).has_value());
    CHECK_FALSE(Intersect(Ray{{-5.0F, 0.0F, -1.5F}, {1.0F, 0.0F, 0.0F}}, box).has_value());
    // Negative zero direction components behave like zero.
    const std::optional<float> negativeZero = Intersect(Ray{{-5.0F, 0.0F, 0.0F}, {1.0F, -0.0F, -0.0F}}, box);
    REQUIRE(negativeZero.has_value());
    CHECK(*negativeZero == 4.0F);
    // Grazing a face and an edge counts as a hit.
    const std::optional<float> grazeFace = Intersect(Ray{{-5.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, box);
    REQUIRE(grazeFace.has_value());
    CHECK(*grazeFace == 4.0F);
    const std::optional<float> grazeEdge = Intersect(Ray{{-5.0F, 1.0F, -1.0F}, {1.0F, 0.0F, 0.0F}}, box);
    REQUIRE(grazeEdge.has_value());
    CHECK(*grazeEdge == 4.0F);
    // Diagonal through the corner (1,1,1) from (3,3,3).
    const std::optional<float> corner = Intersect(Ray{{3.0F, 3.0F, 3.0F}, {-1.0F, -1.0F, -1.0F}}, box);
    REQUIRE(corner.has_value());
    CHECK(*corner == 2.0F);
    // Diagonal miss: x enters at t = 4, y leaves at t = 1.
    CHECK_FALSE(Intersect(Ray{{-5.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 0.0F}}, box).has_value());
    // Oblique hit: from (-3,-2,0) along (1,1,0): x enters at 2, y at 1 -> t = 2.
    const std::optional<float> oblique = Intersect(Ray{{-3.0F, -2.0F, 0.0F}, {1.0F, 1.0F, 0.0F}}, box);
    REQUIRE(oblique.has_value());
    CHECK(*oblique == 2.0F);
    // Box entirely behind.
    CHECK_FALSE(Intersect(Ray{{5.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, box).has_value());
}

TEST_CASE("Ray-Triangle intersection") {
    const Triangle triangle{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    // Two-sided: hits from both sides.
    const std::optional<float> front = Intersect(Ray{{0.25F, 0.25F, -1.0F}, {0.0F, 0.0F, 1.0F}}, triangle);
    REQUIRE(front.has_value());
    CHECK(*front == 1.0F);
    const std::optional<float> back = Intersect(Ray{{0.25F, 0.25F, 2.0F}, {0.0F, 0.0F, -1.0F}}, triangle);
    REQUIRE(back.has_value());
    CHECK(*back == 2.0F);
    const std::optional<float> doubled = Intersect(Ray{{0.25F, 0.25F, -1.0F}, {0.0F, 0.0F, 2.0F}}, triangle);
    REQUIRE(doubled.has_value());
    CHECK(*doubled == 0.5F);
    // Oblique hit: from (0,0,-1) along (0.25,0.25,1) reaches (0.25,0.25,0) at t = 1.
    const std::optional<float> oblique = Intersect(Ray{{0.0F, 0.0F, -1.0F}, {0.25F, 0.25F, 1.0F}}, triangle);
    REQUIRE(oblique.has_value());
    CHECK(*oblique == doctest::Approx(1.0F).epsilon(1e-6));
    // Miss outside the hypotenuse and outside the legs.
    CHECK_FALSE(Intersect(Ray{{0.75F, 0.75F, -1.0F}, {0.0F, 0.0F, 1.0F}}, triangle).has_value());
    CHECK_FALSE(Intersect(Ray{{-0.1F, 0.5F, -1.0F}, {0.0F, 0.0F, 1.0F}}, triangle).has_value());
    CHECK_FALSE(Intersect(Ray{{0.5F, -0.1F, -1.0F}, {0.0F, 0.0F, 1.0F}}, triangle).has_value());
    // Closed edges and vertices.
    const std::optional<float> edgeAb = Intersect(Ray{{0.5F, 0.0F, -1.0F}, {0.0F, 0.0F, 1.0F}}, triangle);
    REQUIRE(edgeAb.has_value());
    CHECK(*edgeAb == 1.0F);
    const std::optional<float> edgeAc = Intersect(Ray{{0.0F, 0.5F, -1.0F}, {0.0F, 0.0F, 1.0F}}, triangle);
    REQUIRE(edgeAc.has_value());
    CHECK(*edgeAc == 1.0F);
    const std::optional<float> edgeBc = Intersect(Ray{{0.5F, 0.5F, -1.0F}, {0.0F, 0.0F, 1.0F}}, triangle);
    REQUIRE(edgeBc.has_value());
    CHECK(*edgeBc == 1.0F);
    const std::optional<float> vertex = Intersect(Ray{{0.0F, 0.0F, -1.0F}, {0.0F, 0.0F, 1.0F}}, triangle);
    REQUIRE(vertex.has_value());
    CHECK(*vertex == 1.0F);
    // Parallel (even in the plane) and behind never hit.
    CHECK_FALSE(Intersect(Ray{{0.25F, 0.25F, 1.0F}, {1.0F, 0.0F, 0.0F}}, triangle).has_value());
    CHECK_FALSE(Intersect(Ray{{-1.0F, 0.25F, 0.0F}, {1.0F, 0.0F, 0.0F}}, triangle).has_value());
    CHECK_FALSE(Intersect(Ray{{0.25F, 0.25F, 1.0F}, {0.0F, 0.0F, 1.0F}}, triangle).has_value());
    // Origin on the triangle hits at 0.
    const std::optional<float> onTriangle = Intersect(Ray{{0.25F, 0.25F, 0.0F}, {0.0F, 0.0F, 1.0F}}, triangle);
    REQUIRE(onTriangle.has_value());
    CHECK(*onTriangle == 0.0F);
    // Winding does not matter.
    const Triangle flipped{triangle.a, triangle.c, triangle.b};
    const std::optional<float> flippedHit = Intersect(Ray{{0.25F, 0.25F, -1.0F}, {0.0F, 0.0F, 1.0F}}, flipped);
    REQUIRE(flippedHit.has_value());
    CHECK(*flippedHit == 1.0F);

    // Agreement with the plane intersection for interior hits.
    Lcg random{271828U};
    for (int i = 0; i < 200; ++i) {
        const Triangle t{random.RangeVec3(-5.0F, 5.0F), random.RangeVec3(-5.0F, 5.0F), random.RangeVec3(-5.0F, 5.0F)};
        if (Area(t) < 1.0F) continue;
        const Vec3 target = t.a + (t.b - t.a) * 0.3F + (t.c - t.a) * 0.3F;
        const float height = random.Range(1.0F, 5.0F);
        const Vec3 jitter = random.RangeVec3(-0.5F, 0.5F);
        const Vec3 origin = target + UnitNormal(t) * height + jitter;
        const Ray ray{origin, (target - origin) * random.Range(0.5F, 2.0F)};
        const std::optional<float> triangleHit = Intersect(ray, t);
        const std::optional<float> planeHit = Intersect(ray, MakePlane(t.a, t.b, t.c));
        REQUIRE(triangleHit.has_value());
        REQUIRE(planeHit.has_value());
        CHECK(*triangleHit == doctest::Approx(*planeHit).epsilon(1e-4));
        CHECK_VEC3_NEAR(PointAt(ray, *triangleHit), target, 1e-3);
    }
}

TEST_CASE("Ray intersections reject non-finite rays and overflow") {
    // Contract: a non-finite origin or direction, or a computation that
    // overflows to NaN, returns nullopt rather than a NaN or false hit.
    const Plane plane{{0.0F, 1.0F, 0.0F}, 0.0F};
    const Sphere sphere{{0.0F, 0.0F, 0.0F}, 1.0F};
    const Aabb box{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    const Triangle triangle{{-1.0F, 0.0F, -1.0F}, {1.0F, 0.0F, -1.0F}, {0.0F, 0.0F, 1.0F}};
    const std::array<Ray, 5> invalid{
        Ray{{kNaN, 2.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
        Ray{{0.0F, 2.0F, 0.0F}, {0.0F, kNaN, 0.0F}},
        Ray{{kInf, 2.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
        Ray{{0.0F, 2.0F, 0.0F}, {0.0F, -kInf, 0.0F}},
        Ray{{-kInf, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}},
    };
    for (std::size_t index = 0; index < invalid.size(); ++index) {
        CAPTURE(index);
        CHECK_FALSE(Intersect(invalid[index], plane).has_value());
        CHECK_FALSE(Intersect(invalid[index], sphere).has_value());
        CHECK_FALSE(Intersect(invalid[index], box).has_value());
        CHECK_FALSE(Intersect(invalid[index], triangle).has_value());
    }

    // Finite input whose squared offset overflows has no answer.
    CHECK_FALSE(Intersect(Ray{{-1e20F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, sphere).has_value());
    // A long direction is fine: the discriminant avoids halfB^2 - a*c, so a
    // ray from -1e10 with direction 1e10 reaches the sphere at t ~ 1.
    const auto scaled = Intersect(Ray{{-1e10F, 0.0F, 0.0F}, {1e10F, 0.0F, 0.0F}}, sphere);
    REQUIRE(scaled.has_value());
    CHECK(*scaled == doctest::Approx(1.0F));
    // Large but representable input still hits.
    const auto distant = Intersect(Ray{{-1e4F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, sphere);
    REQUIRE(distant.has_value());
    CHECK(*distant == doctest::Approx(1e4F - 1.0F));
}

namespace {

struct ReferencePoint final {
    double x{};
    double y{};
    double z{};
};

ReferencePoint ToReference(const Vec3 v) {
    return {static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z)};
}

ReferencePoint Sub(const ReferencePoint a, const ReferencePoint b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double DotReference(const ReferencePoint a, const ReferencePoint b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
ReferencePoint CrossReference(const ReferencePoint a, const ReferencePoint b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
ReferencePoint OnSegment(const ReferencePoint p, const ReferencePoint a, const ReferencePoint b) {
    const ReferencePoint edge = Sub(b, a);
    const double squared = DotReference(edge, edge);
    double t = squared > 0.0 ? DotReference(Sub(p, a), edge) / squared : 0.0;
    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    return {a.x + edge.x * t, a.y + edge.y * t, a.z + edge.z * t};
}

// Double-precision reference by a different method: project onto the plane,
// accept it when it lies inside all three edges, else take the closest edge point.
ReferencePoint ReferenceClosest(const Vec3 point, const Triangle& triangle) {
    const ReferencePoint p = ToReference(point);
    const ReferencePoint a = ToReference(triangle.a);
    const ReferencePoint b = ToReference(triangle.b);
    const ReferencePoint c = ToReference(triangle.c);
    const ReferencePoint normal = CrossReference(Sub(b, a), Sub(c, a));
    const double normalSquared = DotReference(normal, normal);
    if (normalSquared > 0.0) {
        const double along = DotReference(normal, Sub(p, a)) / normalSquared;
        const ReferencePoint projected{p.x - normal.x * along, p.y - normal.y * along, p.z - normal.z * along};
        const bool inside = DotReference(CrossReference(Sub(b, a), Sub(projected, a)), normal) >= 0.0 &&
                            DotReference(CrossReference(Sub(c, b), Sub(projected, b)), normal) >= 0.0 &&
                            DotReference(CrossReference(Sub(a, c), Sub(projected, c)), normal) >= 0.0;
        if (inside) return projected;
    }
    const std::array<ReferencePoint, 3> candidates{OnSegment(p, a, b), OnSegment(p, b, c), OnSegment(p, c, a)};
    ReferencePoint best = candidates[0];
    double bestSquared = DotReference(Sub(p, best), Sub(p, best));
    for (const ReferencePoint& candidate : candidates) {
        const double squared = DotReference(Sub(p, candidate), Sub(p, candidate));
        if (squared < bestSquared) {
            best = candidate;
            bestSquared = squared;
        }
    }
    return best;
}

double ReferenceDistance(const Vec3 actual, const ReferencePoint expected) {
    const ReferencePoint delta = Sub(ToReference(actual), expected);
    return std::sqrt(DotReference(delta, delta));
}

} // namespace

TEST_CASE("Triangle ClosestPoint matches a double-precision reference, including thin triangles") {
    // General triangles, then triangles whose apex sits a decreasing height above
    // the base: from well-shaped through the conditioning threshold
    // (sin(angle)^2 ~ FLT_EPSILON) to collinear. The error bound scales with the
    // problem size and with sqrt(FLT_EPSILON) near the threshold.
    Lcg random{4242U};
    double worstGeneral = 0.0;
    for (int i = 0; i < 4000; ++i) {
        const Triangle triangle{random.RangeVec3(-5.0F, 5.0F), random.RangeVec3(-5.0F, 5.0F),
                                random.RangeVec3(-5.0F, 5.0F)};
        const Vec3 p = random.RangeVec3(-8.0F, 8.0F);
        const double error = ReferenceDistance(ClosestPoint(p, triangle), ReferenceClosest(p, triangle));
        worstGeneral = (std::max)(worstGeneral, error);
    }
    INFO("worst general error " << worstGeneral);
    CHECK(worstGeneral <= 1e-4);

    const std::array<float, 7> heights{1.0F, 1e-2F, 1e-3F, 3e-4F, 1e-4F, 1e-6F, 0.0F};
    for (const float height : heights) {
        double worst = 0.0;
        for (int i = 0; i < 2000; ++i) {
            const Vec3 a = random.RangeVec3(-1.0F, 1.0F);
            const Vec3 b = a + Vec3{4.0F, 0.0F, 0.0F};
            const Vec3 c = a + Vec3{random.Range(0.0F, 4.0F), height, 0.0F};
            const Vec3 p = a + random.RangeVec3(-3.0F, 7.0F);
            const std::array<Triangle, 3> orders{Triangle{a, b, c}, Triangle{b, c, a}, Triangle{c, a, b}};
            for (const Triangle& triangle : orders) {
                const Vec3 actual = ClosestPoint(p, triangle);
                REQUIRE(IsFinite(actual));
                worst = (std::max)(worst, ReferenceDistance(actual, ReferenceClosest(p, triangle)));
            }
        }
        CAPTURE(height);
        INFO("worst thin-triangle error " << worst);
        CHECK(worst <= 2e-3);
    }
}

TEST_CASE("Clamp with hi < lo is a Programmer Error") {
    GYO_CHECK_ASSERTS(Clamp(1, 5, 0));
    GYO_CHECK_ASSERTS(Clamp(0.5f, 1.0f, 0.0f));
    GYO_CHECK_ASSERTS(Clamp(Vec3{}, Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 1.0f}));
    GYO_CHECK_ASSERTS(ClosestPoint(Vec3{}, Aabb{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 1.0f}}));
    // Equal bounds and NaN bounds are not ordered-by-less-than violations.
    CHECK(Clamp(3, 2, 2) == 2);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(Clamp(0.5f, nan, 1.0f) == 0.5f);
}
