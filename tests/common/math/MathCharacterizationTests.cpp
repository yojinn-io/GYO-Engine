// Characterization of the single Engine::Math implementations against the
// per-module engine helpers they replace (math-foundation plan, batch B1).
//
// Every helper under namespace Legacy is frozen verbatim from master 0bd5363:
// same expression shapes, operand order and types, so that FMA contraction and
// rounding behave as in the original translation unit. Only the minimal
// input types are redeclared locally; no module header is included. Results
// are compared bit for bit (NaN equals NaN regardless of payload) over fixed
// deterministic input grids. Pairs that are not bit-identical by design are
// either bounded drift (measured, bound recorded next to the check) or a
// documented semantic difference that is tested on both sides.
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Rect.hpp"
#include "engine/math/geometry/Segment.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/Vec3d.hpp"
#include "engine/math/scalar/ColorSpace.hpp"
#include "engine/math/scalar/Constants.hpp"

namespace Legacy {
namespace {

// ---------------------------------------------------------------------------
// engine/model. Types from engine/model/include/model/ModelAsset.hpp:14-27.
namespace Model {

struct Vec3 final { float x{}, y{}, z{}; };
struct Quaternion final { float x{}, y{}, z{}, w{1.0F}; };
struct Transform final {
    Vec3 translation{};
    Quaternion rotation{};
    Vec3 scale{1.0F, 1.0F, 1.0F};
};
struct Matrix4 final {
    std::array<float, 16> values{
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

// engine/model/src/ModelAsset.cpp:7-45 at master.
namespace ModelAssetCpp {

Matrix4 Multiply(const Matrix4& a, const Matrix4& b) noexcept {
    Matrix4 result;
    result.values.fill(0);
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t k = 0; k < 4; ++k) {
                result.values[column * 4 + row] +=
                    a.values[k * 4 + row] * b.values[column * 4 + k];
            }
        }
    }
    return result;
}

Matrix4 ToMatrix(const Transform& transform) noexcept {
    const auto& q = transform.rotation;
    const float lengthSquared = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    const float s = lengthSquared > 0 ? 2.0F / lengthSquared : 0.0F;
    const float xx=q.x*q.x*s, xy=q.x*q.y*s, xz=q.x*q.z*s;
    const float yy=q.y*q.y*s, yz=q.y*q.z*s, zz=q.z*q.z*s;
    const float wx=q.w*q.x*s, wy=q.w*q.y*s, wz=q.w*q.z*s;
    Matrix4 result;
    result.values = {
        (1-yy-zz)*transform.scale.x, (xy+wz)*transform.scale.x,
        (xz-wy)*transform.scale.x, 0,
        (xy-wz)*transform.scale.y, (1-xx-zz)*transform.scale.y,
        (yz+wx)*transform.scale.y, 0,
        (xz+wy)*transform.scale.z, (yz-wx)*transform.scale.z,
        (1-xx-yy)*transform.scale.z, 0,
        transform.translation.x, transform.translation.y, transform.translation.z, 1};
    return result;
}

Vec3 TransformPoint(const Matrix4& matrix, const Vec3 point) noexcept {
    const auto& m=matrix.values;
    return {m[0]*point.x+m[4]*point.y+m[8]*point.z+m[12],
            m[1]*point.x+m[5]*point.y+m[9]*point.z+m[13],
            m[2]*point.x+m[6]*point.y+m[10]*point.z+m[14]};
}

} // namespace ModelAssetCpp

// engine/model/src/Animation.cpp:12,23-46 at master. Finite(Quaternion) at
// :13 is a Model validity rule (length > 1e-12) and stays in Model.
namespace AnimationCpp {

bool Finite(const Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
// engine/model/src/Animation.cpp:18 at master.
bool Finite(const Matrix4& m) {
    return std::all_of(m.values.begin(),m.values.end(),[](float f){return std::isfinite(f);});
}

Vec3 Lerp(const Vec3 a,const Vec3 b,const float t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}

Quaternion Normalize(Quaternion q) {
    const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if (length<=1.0e-12F) return {};
    return {q.x/length,q.y/length,q.z/length,q.w/length};
}

Quaternion Slerp(Quaternion a,Quaternion b,const float t) {
    a=Normalize(a); b=Normalize(b);
    float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if (dot<0) { b={-b.x,-b.y,-b.z,-b.w}; dot=-dot; }
    dot=std::clamp(dot,-1.0F,1.0F);
    float left=1-t,right=t;
    if (dot<0.9995F) {
        const float angle=std::acos(dot),divisor=std::sin(angle);
        left=std::sin((1-t)*angle)/divisor;
        right=std::sin(t*angle)/divisor;
    }
    return Normalize({a.x*left+b.x*right,a.y*left+b.y*right,
                      a.z*left+b.z*right,a.w*left+b.w*right});
}

} // namespace AnimationCpp

// engine/model/src/AnimationTransfer.cpp:12-25 at master.
namespace AnimationTransferCpp {

Quaternion Normalize(const Quaternion q) {
    const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return {q.x/length,q.y/length,q.z/length,q.w/length};
}
Quaternion Inverse(const Quaternion q) { return {-q.x,-q.y,-q.z,q.w}; }
Quaternion Product(const Quaternion a,const Quaternion b) {
    return Normalize({a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
                      a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                      a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
                      a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z});
}
bool Finite(const Vec3 v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}

} // namespace AnimationTransferCpp

} // namespace Model

// ---------------------------------------------------------------------------
// engine/collision. Float3 from engine/collision/include/engine/collision/Collision.hpp:7.
namespace Collision {

struct Float3 final { float x{}, y{}, z{}; };

// engine/collision/src/Collision.cpp:14-20,51-54 at master.
namespace CollisionCpp {

[[nodiscard]] bool IsFinite(const Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] float Length(const Float3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] Float3 Normalize(const Float3 value) noexcept {
    const float length = Length(value);
    return {value.x / length, value.y / length, value.z / length};
}

} // namespace CollisionCpp

// engine/collision/src/CapsuleQueries.cpp:13-34 at master (kTolerance omitted).
namespace CapsuleQueriesCpp {

// Calculations use doubles to retain accuracy for short sweeps and long rays.
struct Vec3 {
    double x{}, y{}, z{};
};
Vec3 V(Float3 v) { return {v.x, v.y, v.z}; }
Float3 F(Vec3 v) {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, double b) { return {a.x * b, a.y * b, a.z * b}; }
double Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
bool Finite(Float3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
Vec3 Closest(Vec3 p, Vec3 a, Vec3 b) {
    const Vec3 edge = b - a;
    const double squared = Dot(edge, edge);
    return a + edge * (squared > 0.0 ? std::clamp(Dot(p - a, edge) / squared, 0.0, 1.0) : 0.0);
}
Vec3 Clamp(Vec3 p, Vec3 lo, Vec3 hi) {
    return {std::clamp(p.x, lo.x, hi.x), std::clamp(p.y, lo.y, hi.y), std::clamp(p.z, lo.z, hi.z)};
}

} // namespace CapsuleQueriesCpp

} // namespace Collision

// ---------------------------------------------------------------------------
// engine/render. Types from engine/render/include/render/RenderTypes.hpp:12-55,
// 145-153 (only the fields the frozen helpers read) and ShaderAbi.hpp:5.
namespace Render {

struct Float2 final {
    float x{};
    float y{};
};

struct Float3 final {
    float x{};
    float y{};
    float z{};
};

struct Rect final {
    float x{};
    float y{};
    float width{};
    float height{};
};

struct Transform3D final {
    Float3 translation{};
    Float3 rotationRadians{};
    Float3 scale{1.0F, 1.0F, 1.0F};
};

struct PerspectiveCamera3D final {
    Float3 position{};
    Float3 rotationRadians{};
    float verticalFieldOfViewRadians{1.0471975512F};
    float nearClip{0.05F};
    float farClip{100.0F};
};

struct SpriteSubmission final {
    Rect destinationPixels{};
    Float2 pivotNormalized{};
    float rotationRadians{};
};

// ShaderAbi::Matrix4: row-major storage, row vectors (p' = p * M).
struct LegacyMatrix4 final { float values[4][4]{}; };

// engine/render/src/Renderer.cpp:17-166 at master.
namespace RendererCpp {

using Matrix4 = LegacyMatrix4;

[[nodiscard]] Matrix4 Identity() noexcept {
    Matrix4 result{};
    result.values[0][0] = 1.0F;
    result.values[1][1] = 1.0F;
    result.values[2][2] = 1.0F;
    result.values[3][3] = 1.0F;
    return result;
}

[[nodiscard]] Matrix4 Multiply(const Matrix4& left, const Matrix4& right) noexcept {
    Matrix4 result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result.values[row][column] +=
                    left.values[row][inner] * right.values[inner][column];
            }
        }
    }
    return result;
}

[[nodiscard]] Matrix4 Translation(Float3 value) noexcept {
    Matrix4 result = Identity();
    result.values[3][0] = value.x;
    result.values[3][1] = value.y;
    result.values[3][2] = value.z;
    return result;
}

[[nodiscard]] Matrix4 Scale(Float3 value) noexcept {
    Matrix4 result = Identity();
    result.values[0][0] = value.x;
    result.values[1][1] = value.y;
    result.values[2][2] = value.z;
    return result;
}

[[nodiscard]] Matrix4 RotationX(float radians) noexcept {
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[1][1] = cosine;
    result.values[1][2] = sine;
    result.values[2][1] = -sine;
    result.values[2][2] = cosine;
    return result;
}

[[nodiscard]] Matrix4 RotationY(float radians) noexcept {
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0][0] = cosine;
    result.values[0][2] = -sine;
    result.values[2][0] = sine;
    result.values[2][2] = cosine;
    return result;
}

[[nodiscard]] Matrix4 RotationZ(float radians) noexcept {
    Matrix4 result = Identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0][0] = cosine;
    result.values[0][1] = sine;
    result.values[1][0] = -sine;
    result.values[1][1] = cosine;
    return result;
}

[[nodiscard]] Matrix4 WorldMatrix(const Transform3D& transform) noexcept {
    Matrix4 result = Scale(transform.scale);
    result = Multiply(result, RotationX(transform.rotationRadians.x));
    result = Multiply(result, RotationY(transform.rotationRadians.y));
    result = Multiply(result, RotationZ(transform.rotationRadians.z));
    return Multiply(result, Translation(transform.translation));
}

[[nodiscard]] Matrix4 ViewMatrix(const PerspectiveCamera3D& camera) noexcept {
    Matrix4 result = Translation({
        -camera.position.x,
        -camera.position.y,
        -camera.position.z,
    });
    result = Multiply(result, RotationZ(-camera.rotationRadians.z));
    result = Multiply(result, RotationY(-camera.rotationRadians.y));
    return Multiply(result, RotationX(-camera.rotationRadians.x));
}

[[nodiscard]] Matrix4 ProjectionMatrix(
    const PerspectiveCamera3D& camera,
    float aspectRatio) noexcept {
    Matrix4 result{};
    const float yScale = 1.0F /
                         std::tan(camera.verticalFieldOfViewRadians * 0.5F);
    const float xScale = yScale / aspectRatio;
    const float depthRange = camera.farClip - camera.nearClip;
    result.values[0][0] = xScale;
    result.values[1][1] = yScale;
    result.values[2][2] = camera.farClip / depthRange;
    result.values[2][3] = 1.0F;
    result.values[3][2] =
        -(camera.nearClip * camera.farClip) / depthRange;
    return result;
}

[[nodiscard]] Matrix4 SpriteWorldMatrix(
    const SpriteSubmission& sprite) noexcept {
    const Float3 localPivotTranslation{
        0.5F - sprite.pivotNormalized.x,
        0.5F - sprite.pivotNormalized.y,
        0.0F,
    };
    const Float3 anchor{
        sprite.destinationPixels.x +
            sprite.pivotNormalized.x * sprite.destinationPixels.width,
        sprite.destinationPixels.y +
            sprite.pivotNormalized.y * sprite.destinationPixels.height,
        0.0F,
    };

    Matrix4 result = Translation(localPivotTranslation);
    result = Multiply(result, Scale({
        sprite.destinationPixels.width,
        sprite.destinationPixels.height,
        1.0F,
    }));
    result = Multiply(result, RotationZ(sprite.rotationRadians));
    return Multiply(result, Translation(anchor));
}

[[nodiscard]] Matrix4 PixelProjection(float width, float height) noexcept {
    Matrix4 result{};
    result.values[0][0] = 2.0F / width;
    result.values[1][1] = -2.0F / height;
    result.values[2][2] = 1.0F;
    result.values[3][0] = -1.0F;
    result.values[3][1] = 1.0F;
    result.values[3][3] = 1.0F;
    return result;
}

[[nodiscard]] bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool IsFinite(Float3 value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

} // namespace RendererCpp

// engine/render/src/PrimitiveMesh.cpp:29-53 at master.
namespace PrimitiveMeshCpp {

[[nodiscard]] Float3 Add(Float3 a, Float3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] Float3 Subtract(Float3 a, Float3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] Float3 Scale(Float3 a, float scale) noexcept {
    return {a.x * scale, a.y * scale, a.z * scale};
}

[[nodiscard]] Float3 Cross(Float3 a, Float3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

[[nodiscard]] float Length(Float3 value) noexcept {
    return std::hypot(value.x, value.y, value.z);
}

[[nodiscard]] bool IsFinite(Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

// Inline normalization pattern at PrimitiveMesh.cpp:59 (Perpendicular) and
// :66 (AppendWireSegment): multiply by the reciprocal of the hypot length.
[[nodiscard]] Float3 ScaleByReciprocalLength(Float3 value) noexcept {
    return Scale(value, 1.0F / Length(value));
}

} // namespace PrimitiveMeshCpp

// engine/render/src/RenderQueue.cpp:11-31 at master (IsFinite(Color) at :23
// has no Math counterpart: Render::Color stays in Render).
namespace RenderQueueCpp {

[[nodiscard]] bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool IsFinite(Float2 value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y);
}

[[nodiscard]] bool IsFinite(Float3 value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

[[nodiscard]] bool IsFinite(Rect value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y) &&
           IsFinite(value.width) && IsFinite(value.height);
}

} // namespace RenderQueueCpp

// engine/render/backend/sdl_gpu/src/SdlGpuRenderDevice.cpp:17-23 at master
// (IsFinite(Color) at :25 stays with Render::Color).
namespace SdlGpuRenderDeviceCpp {

[[nodiscard]] bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool IsFinite(Float3 value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

} // namespace SdlGpuRenderDeviceCpp

// engine/render/src/ColorTransform.cpp:9-11,23-35 at master.
namespace ColorTransformCpp {

[[nodiscard]] float ClampUnit(const float value) noexcept {
    return std::clamp(value, 0.0F, 1.0F);
}

float DecodeSrgbComponent(const float encoded) noexcept {
    const float value = ClampUnit(encoded);
    return value <= 0.04045F
        ? value / 12.92F
        : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

float EncodeSrgbComponent(const float linear) noexcept {
    const float value = ClampUnit(linear);
    return value <= 0.0031308F
        ? value * 12.92F
        : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
}

} // namespace ColorTransformCpp

} // namespace Render

// ---------------------------------------------------------------------------
// engine/ui. Types from engine/ui/include/ui/UiTypes.hpp:40-50.
namespace Ui {

struct UiFloat2 final {
    float x{};
    float y{};
};

struct UiRect final {
    float x{};
    float y{};
    float width{};
    float height{};
};

// engine/ui/src/UiColor.cpp:24-35 at master; kByteScale from :76 is how
// DecodeSrgbHexColor forms each channel (channel * kByteScale).
namespace UiColorCpp {

[[nodiscard]] float SrgbToLinear(float value) noexcept {
    return value <= 0.04045F
        ? value / 12.92F
        : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

[[nodiscard]] float LinearToSrgb(float value) noexcept {
    value = std::clamp(value, 0.0F, 1.0F);
    return value <= 0.0031308F
        ? value * 12.92F
        : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
}

constexpr float kByteScale = 1.0F / 255.0F;

} // namespace UiColorCpp

// engine/ui/src/UiRuntime.cpp:337-348 at master.
namespace UiRuntimeCpp {

[[nodiscard]] UiRect IntersectRect(UiRect left, UiRect right) noexcept {
    const float x = std::max(left.x, right.x);
    const float y = std::max(left.y, right.y);
    const float rightEdge = std::min(left.x + left.width, right.x + right.width);
    const float bottomEdge = std::min(left.y + left.height, right.y + right.height);
    return {
        x,
        y,
        std::max(0.0F, rightEdge - x),
        std::max(0.0F, bottomEdge - y),
    };
}

} // namespace UiRuntimeCpp

// engine/ui/src/UiValidation.cpp:24-26 at master (IsFinite(UiColor) at :28
// stays with UiColor).
namespace UiValidationCpp {

[[nodiscard]] bool IsFinite(UiFloat2 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

} // namespace UiValidationCpp

} // namespace Ui

} // namespace
} // namespace Legacy

namespace {

namespace Math = Engine::Math;

constexpr float kInfinity = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// Signed zeros, subnormals, the normal limits, values whose squares overflow
// float (1e19 squared is near FLT_MAX, 1e30 overflows), infinities and NaN.
constexpr std::array<float, 17> kSpecialFloats{
    0.0F, -0.0F,
    std::numeric_limits<float>::denorm_min(), -std::numeric_limits<float>::denorm_min(),
    FLT_MIN, -FLT_MIN,
    1.0F, -1.0F, 0.5F,
    1.0e19F, -1.0e19F, 1.0e30F, -1.0e30F,
    FLT_MAX,
    kInfinity, -kInfinity, kNaN,
};

// Fixed-seed linear congruential generator (Numerical Recipes constants).
class Lcg final {
public:
    explicit Lcg(const std::uint32_t seed) noexcept : state_(seed) {}

    std::uint32_t Next() noexcept {
        state_ = state_ * 1664525U + 1013904223U;
        return state_;
    }

    // Uniform in [lo, hi] using the top 24 bits.
    float Uniform(const float lo, const float hi) noexcept {
        const float unit = static_cast<float>(Next() >> 8U) * (1.0F / 16777216.0F);
        return lo + (hi - lo) * unit;
    }

    std::size_t Index(const std::size_t count) noexcept { return static_cast<std::size_t>(Next() % count); }

private:
    std::uint32_t state_;
};

[[nodiscard]] bool SameBits(const float a, const float b) noexcept {
    if (std::isnan(a) && std::isnan(b)) return true;
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

[[nodiscard]] bool SameBits(const double a, const double b) noexcept {
    if (std::isnan(a) && std::isnan(b)) return true;
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

// Distance in representable floats; -0 and +0 are 0 apart. NaN against a
// number is reported as the largest distance.
[[nodiscard]] std::int64_t UlpDistance(const float a, const float b) noexcept {
    if (std::isnan(a) || std::isnan(b)) {
        return std::isnan(a) && std::isnan(b) ? 0 : std::numeric_limits<std::int64_t>::max();
    }
    const auto ordered = [](const float value) {
        const auto bits = static_cast<std::int64_t>(std::bit_cast<std::int32_t>(value));
        return bits < 0 ? std::int64_t{std::numeric_limits<std::int32_t>::min()} - bits : bits;
    };
    return std::llabs(ordered(a) - ordered(b));
}

[[nodiscard]] std::string Describe(const float value) {
    std::ostringstream stream;
    stream << std::hexfloat << value;
    return stream.str();
}

[[nodiscard]] std::string Describe(const double value) {
    std::ostringstream stream;
    stream << std::hexfloat << value;
    return stream.str();
}

// Counts bitwise mismatches and keeps the first one for the failure message.
struct BitTally final {
    std::size_t compared{};
    std::size_t mismatched{};
    std::string first;

    template<class T>
    void Compare(const T legacy, const T math, const std::size_t caseIndex, const int component) {
        ++compared;
        if (SameBits(legacy, math)) return;
        if (mismatched++ == 0) {
            first = "case " + std::to_string(caseIndex) + " component " + std::to_string(component) +
                    ": legacy " + Describe(legacy) + " math " + Describe(math);
        }
    }

    void CompareBool(const bool legacy, const bool math, const std::size_t caseIndex) {
        ++compared;
        if (legacy == math) return;
        if (mismatched++ == 0) {
            first = "case " + std::to_string(caseIndex) + ": legacy " + (legacy ? "true" : "false") +
                    " math " + (math ? "true" : "false");
        }
    }
};

void RequireIdentical(const BitTally& tally) {
    INFO("first mismatch: " << tally.first);
    CHECK(tally.compared > 0);
    CHECK(tally.mismatched == 0);
}

// ---------------------------------------------------------------------------
// Input grids.

using Float3Input = std::array<float, 3>;
using Float4Input = std::array<float, 4>;

// Random [-100, 100] triples, every special value in every component, the
// all-special diagonal and mixed special triples.
[[nodiscard]] std::vector<Float3Input> Float3Grid(const std::uint32_t seed) {
    Lcg random{seed};
    std::vector<Float3Input> grid;
    for (int i = 0; i < 256; ++i) {
        grid.push_back({random.Uniform(-100.0F, 100.0F), random.Uniform(-100.0F, 100.0F),
                        random.Uniform(-100.0F, 100.0F)});
    }
    for (const float special : kSpecialFloats) {
        for (std::size_t component = 0; component < 3; ++component) {
            Float3Input value{random.Uniform(-4.0F, 4.0F), random.Uniform(-4.0F, 4.0F), random.Uniform(-4.0F, 4.0F)};
            value[component] = special;
            grid.push_back(value);
        }
        grid.push_back({special, special, special});
    }
    for (std::size_t i = 0; i < kSpecialFloats.size(); ++i) {
        for (std::size_t j = 0; j < kSpecialFloats.size(); ++j) {
            grid.push_back({kSpecialFloats[i], kSpecialFloats[j], kSpecialFloats[(i + j) % kSpecialFloats.size()]});
        }
    }
    return grid;
}

// Random [-2, 2] quaternions plus the degenerate and extreme cases the
// guarded and unguarded normalizations treat differently.
[[nodiscard]] std::vector<Float4Input> QuaternionGrid(const std::uint32_t seed) {
    Lcg random{seed};
    std::vector<Float4Input> grid;
    for (int i = 0; i < 256; ++i) {
        grid.push_back({random.Uniform(-2.0F, 2.0F), random.Uniform(-2.0F, 2.0F), random.Uniform(-2.0F, 2.0F),
                        random.Uniform(-2.0F, 2.0F)});
    }
    grid.push_back({0.0F, 0.0F, 0.0F, 1.0F});
    grid.push_back({0.0F, 0.0F, 0.0F, -1.0F});
    grid.push_back({0.0F, 0.0F, 0.0F, 0.0F});
    grid.push_back({-0.0F, 0.0F, -0.0F, 0.0F});
    grid.push_back({1.0e-12F, 0.0F, 0.0F, 0.0F});  // length exactly at the guard
    grid.push_back({1.0e-13F, 1.0e-13F, 1.0e-13F, 1.0e-13F});
    grid.push_back({2.0e-12F, 0.0F, 0.0F, 0.0F});  // just above the guard; square underflows
    grid.push_back({1.0e19F, 1.0e19F, 1.0e19F, 1.0e19F});  // sum of squares overflows
    for (const float special : kSpecialFloats) {
        for (std::size_t component = 0; component < 4; ++component) {
            Float4Input value{random.Uniform(-1.0F, 1.0F), random.Uniform(-1.0F, 1.0F), random.Uniform(-1.0F, 1.0F),
                              random.Uniform(-1.0F, 1.0F)};
            value[component] = special;
            grid.push_back(value);
        }
    }
    return grid;
}

// Random [-4, 4] matrices, then matrices carrying one special value at every
// position in turn.
[[nodiscard]] std::vector<Math::Matrix4> MatrixGrid(const std::uint32_t seed) {
    Lcg random{seed};
    std::vector<Math::Matrix4> grid;
    for (int i = 0; i < 64; ++i) {
        Math::Matrix4 matrix;
        for (float& value : matrix.values) value = random.Uniform(-4.0F, 4.0F);
        grid.push_back(matrix);
    }
    for (std::size_t i = 0; i < kSpecialFloats.size(); ++i) {
        for (std::size_t position = 0; position < 16; ++position) {
            Math::Matrix4 matrix;
            for (float& value : matrix.values) value = random.Uniform(-4.0F, 4.0F);
            matrix.values[position] = kSpecialFloats[(i + position) % kSpecialFloats.size()];
            grid.push_back(matrix);
        }
    }
    return grid;
}

// Angles including the exact float Pi values, a large angle whose sin/cos
// rely on argument reduction, and tiny angles.
constexpr std::array<float, 12> kAngles{
    0.0F, -0.0F, Engine::Math::Pi, -Engine::Math::Pi, Engine::Math::HalfPi, -Engine::Math::HalfPi,
    100.0F, -100.0F, 1.0e-7F, 0.5F, 3.0F, -2.25F,
};

[[nodiscard]] Math::Vec3 ToMath(const Legacy::Model::Vec3 v) { return {v.x, v.y, v.z}; }
[[nodiscard]] Math::Vec3 ToMath(const Legacy::Collision::Float3 v) { return {v.x, v.y, v.z}; }
[[nodiscard]] Math::Vec3 ToMath(const Legacy::Render::Float3 v) { return {v.x, v.y, v.z}; }
[[nodiscard]] Math::Quaternion ToMath(const Legacy::Model::Quaternion q) { return {q.x, q.y, q.z, q.w}; }
[[nodiscard]] Math::Vec3d ToMath(const Legacy::Collision::CapsuleQueriesCpp::Vec3 v) { return {v.x, v.y, v.z}; }

[[nodiscard]] Legacy::Model::Matrix4 ToModel(const Math::Matrix4& matrix) { return {matrix.values}; }

// Row-major [r][c] flattened at r * 4 + c: the 16 floats the GPU receives.
[[nodiscard]] std::array<float, 16> Flatten(const Legacy::Render::LegacyMatrix4& matrix) {
    std::array<float, 16> result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) result[row * 4 + column] = matrix.values[row][column];
    }
    return result;
}

[[nodiscard]] Legacy::Render::LegacyMatrix4 Unflatten(const Math::Matrix4& matrix) {
    Legacy::Render::LegacyMatrix4 result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) result.values[row][column] = matrix.values[row * 4 + column];
    }
    return result;
}

void CompareMatrix(BitTally& tally, const std::array<float, 16>& legacy, const Math::Matrix4& math,
                   const std::size_t caseIndex) {
    for (std::size_t i = 0; i < 16; ++i) tally.Compare(legacy[i], math.values[i], caseIndex, static_cast<int>(i));
}

void CompareMatrix(BitTally& tally, const Legacy::Model::Matrix4& legacy, const Math::Matrix4& math,
                   const std::size_t caseIndex) {
    CompareMatrix(tally, legacy.values, math, caseIndex);
}

void CompareVec3(BitTally& tally, const float x, const float y, const float z, const Math::Vec3 math,
                 const std::size_t caseIndex) {
    tally.Compare(x, math.x, caseIndex, 0);
    tally.Compare(y, math.y, caseIndex, 1);
    tally.Compare(z, math.z, caseIndex, 2);
}

void CompareQuaternion(BitTally& tally, const Legacy::Model::Quaternion legacy, const Math::Quaternion math,
                       const std::size_t caseIndex) {
    tally.Compare(legacy.x, math.x, caseIndex, 0);
    tally.Compare(legacy.y, math.y, caseIndex, 1);
    tally.Compare(legacy.z, math.z, caseIndex, 2);
    tally.Compare(legacy.w, math.w, caseIndex, 3);
}

void CompareVec3d(BitTally& tally, const Legacy::Collision::CapsuleQueriesCpp::Vec3 legacy, const Math::Vec3d math,
                  const std::size_t caseIndex) {
    tally.Compare(legacy.x, math.x, caseIndex, 0);
    tally.Compare(legacy.y, math.y, caseIndex, 1);
    tally.Compare(legacy.z, math.z, caseIndex, 2);
}

// ---------------------------------------------------------------------------
// Render chains rebuilt with Math in the column-vector order of the plan's
// mirror table (row-vector A * B becomes Multiply(B, A)).

[[nodiscard]] Math::Matrix4 MathWorldMatrix(const Legacy::Render::Transform3D& transform) {
    return Math::ComposeEulerXYZ(ToMath(transform.translation), ToMath(transform.rotationRadians),
                                 ToMath(transform.scale));
}

[[nodiscard]] Math::Matrix4 MathViewMatrix(const Legacy::Render::PerspectiveCamera3D& camera) {
    Math::Matrix4 result = Math::MakeTranslation(-ToMath(camera.position));
    result = Math::Multiply(Math::MakeRotationZ(-camera.rotationRadians.z), result);
    result = Math::Multiply(Math::MakeRotationY(-camera.rotationRadians.y), result);
    return Math::Multiply(Math::MakeRotationX(-camera.rotationRadians.x), result);
}

[[nodiscard]] Math::Matrix4 MathProjectionMatrix(const Legacy::Render::PerspectiveCamera3D& camera,
                                                 const float aspectRatio) {
    return Math::MakePerspective(camera.verticalFieldOfViewRadians, aspectRatio, camera.nearClip, camera.farClip);
}

// The pivot and anchor expressions are the caller's (Renderer) and keep the
// legacy shape; only the matrix chain moves to Math.
[[nodiscard]] Math::Matrix4 MathSpriteWorldMatrix(const Legacy::Render::SpriteSubmission& sprite) {
    const Math::Vec3 localPivotTranslation{
        0.5F - sprite.pivotNormalized.x,
        0.5F - sprite.pivotNormalized.y,
        0.0F,
    };
    const Math::Vec3 anchor{
        sprite.destinationPixels.x +
            sprite.pivotNormalized.x * sprite.destinationPixels.width,
        sprite.destinationPixels.y +
            sprite.pivotNormalized.y * sprite.destinationPixels.height,
        0.0F,
    };
    Math::Matrix4 result = Math::MakeTranslation(localPivotTranslation);
    result = Math::Multiply(
        Math::MakeScale({sprite.destinationPixels.width, sprite.destinationPixels.height, 1.0F}), result);
    result = Math::Multiply(Math::MakeRotationZ(sprite.rotationRadians), result);
    return Math::Multiply(Math::MakeTranslation(anchor), result);
}

[[nodiscard]] std::vector<Legacy::Render::Transform3D> TransformGrid() {
    Lcg random{0x7A3D1E55U};
    std::vector<Legacy::Render::Transform3D> grid;
    for (std::size_t i = 0; i < kAngles.size(); ++i) {
        for (std::size_t j = 0; j < kAngles.size(); ++j) {
            Legacy::Render::Transform3D transform;
            transform.translation = {random.Uniform(-50.0F, 50.0F), random.Uniform(-50.0F, 50.0F),
                                     random.Uniform(-50.0F, 50.0F)};
            transform.rotationRadians = {kAngles[i], kAngles[j], kAngles[(i * 5 + j) % kAngles.size()]};
            transform.scale = {random.Uniform(-3.0F, 3.0F), random.Uniform(-3.0F, 3.0F), random.Uniform(-3.0F, 3.0F)};
            grid.push_back(transform);
        }
    }
    // Unit, negative (mirroring) and zero scales.
    grid.push_back({{}, {}, {1.0F, 1.0F, 1.0F}});
    grid.push_back({{1.0F, 2.0F, 3.0F}, {0.5F, -0.25F, 2.0F}, {-1.0F, 1.0F, 1.0F}});
    grid.push_back({{1.0F, 2.0F, 3.0F}, {0.5F, -0.25F, 2.0F}, {-2.0F, -3.0F, -4.0F}});
    grid.push_back({{1.0F, 2.0F, 3.0F}, {0.5F, -0.25F, 2.0F}, {0.0F, 1.0F, 0.0F}});
    return grid;
}

[[nodiscard]] std::vector<Legacy::Render::PerspectiveCamera3D> CameraGrid() {
    constexpr std::array<float, 4> fieldsOfView{1.0471975512F, 0.1F, 3.0F, 1.5707964F};
    constexpr std::array<float, 4> nearClips{0.05F, 1.0e-6F, 1.0e-30F, 10.0F};
    constexpr std::array<float, 3> farClips{100.0F, 1.0e6F, 10.5F};
    Lcg random{0x1B873593U};
    std::vector<Legacy::Render::PerspectiveCamera3D> grid;
    for (std::size_t i = 0; i < kAngles.size(); ++i) {
        for (std::size_t j = 0; j < fieldsOfView.size() * nearClips.size(); ++j) {
            Legacy::Render::PerspectiveCamera3D camera;
            camera.position = {random.Uniform(-100.0F, 100.0F), random.Uniform(-100.0F, 100.0F),
                               random.Uniform(-100.0F, 100.0F)};
            camera.rotationRadians = {kAngles[i], kAngles[(i + j) % kAngles.size()],
                                      kAngles[(i * 3 + j) % kAngles.size()]};
            camera.verticalFieldOfViewRadians = fieldsOfView[j % fieldsOfView.size()];
            camera.nearClip = nearClips[j / fieldsOfView.size()];
            camera.farClip = farClips[(i + j) % farClips.size()];
            grid.push_back(camera);
        }
    }
    return grid;
}

[[nodiscard]] std::vector<Legacy::Render::SpriteSubmission> SpriteGrid() {
    constexpr std::array<std::array<float, 2>, 6> pivots{{
        {0.0F, 0.0F}, {0.5F, 0.5F}, {1.0F, 1.0F}, {-0.25F, 1.75F}, {0.3F, 0.9F}, {1.0F, 0.0F},
    }};
    Lcg random{0xCC9E2D51U};
    std::vector<Legacy::Render::SpriteSubmission> grid;
    for (std::size_t i = 0; i < kAngles.size(); ++i) {
        for (const auto& pivot : pivots) {
            Legacy::Render::SpriteSubmission sprite;
            sprite.destinationPixels = {random.Uniform(-500.0F, 2000.0F), random.Uniform(-500.0F, 2000.0F),
                                        random.Uniform(-100.0F, 1000.0F), random.Uniform(-100.0F, 1000.0F)};
            sprite.pivotNormalized = {pivot[0], pivot[1]};
            sprite.rotationRadians = kAngles[i];
            grid.push_back(sprite);
        }
    }
    grid.push_back({{0.0F, 0.0F, 0.0F, 0.0F}, {0.5F, 0.5F}, 0.0F});
    grid.push_back({{10.0F, 20.0F, 64.0F, 32.0F}, {0.5F, 0.5F}, 1.0e-7F});
    return grid;
}

constexpr std::array<float, 3> kAspectRatios{16.0F / 9.0F, 1.0F, 0.5F};

constexpr std::array<std::array<float, 2>, 7> kPixelSizes{{
    {1920.0F, 1080.0F}, {1.0F, 1.0F}, {3.0F, 7.0F}, {0.5F, 0.25F}, {7680.0F, 4320.0F}, {0.0F, 0.0F},
    {-640.0F, 480.0F},
}};

} // namespace

// ===========================================================================
// engine/model

TEST_CASE("characterization: Model Multiply equals Math Multiply bit for bit") {
    const auto grid = MatrixGrid(0x2545F491U);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const Math::Matrix4& a = grid[i];
        const Math::Matrix4& b = grid[(i * 7 + 3) % grid.size()];
        CompareMatrix(tally, Legacy::Model::ModelAssetCpp::Multiply(ToModel(a), ToModel(b)), Math::Multiply(a, b), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Model ToMatrix equals Math ComposeTRS bit for bit") {
    const auto rotations = QuaternionGrid(0x68E31DA4U);
    const auto vectors = Float3Grid(0xB5297A4DU);
    BitTally tally;
    for (std::size_t i = 0; i < rotations.size(); ++i) {
        const auto& r = rotations[i];
        const auto& t = vectors[i % vectors.size()];
        const auto& s = vectors[(i * 13 + 5) % vectors.size()];
        const Legacy::Model::Transform transform{{t[0], t[1], t[2]}, {r[0], r[1], r[2], r[3]}, {s[0], s[1], s[2]}};
        CompareMatrix(tally, Legacy::Model::ModelAssetCpp::ToMatrix(transform),
                      Math::ComposeTRS(ToMath(transform.translation), ToMath(transform.rotation),
                                       ToMath(transform.scale)),
                      i);
    }
    // The zero quaternion takes the s = 0 branch on both sides (scale only).
    const Legacy::Model::Transform zero{{1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, 0.0F, 0.0F}, {2.0F, 3.0F, 4.0F}};
    CompareMatrix(tally, Legacy::Model::ModelAssetCpp::ToMatrix(zero),
                  Math::ComposeTRS({1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, 0.0F, 0.0F}, {2.0F, 3.0F, 4.0F}),
                  rotations.size());
    RequireIdentical(tally);
}

TEST_CASE("characterization: Model TransformPoint equals Math TransformPoint bit for bit") {
    const auto matrices = MatrixGrid(0x9E3779B9U);
    const auto points = Float3Grid(0x85EBCA6BU);
    BitTally tally;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Math::Matrix4& matrix = matrices[(i * 11) % matrices.size()];
        const auto& p = points[i];
        const auto legacy = Legacy::Model::ModelAssetCpp::TransformPoint(ToModel(matrix), {p[0], p[1], p[2]});
        CompareVec3(tally, legacy.x, legacy.y, legacy.z, Math::TransformPoint(matrix, {p[0], p[1], p[2]}), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Animation Lerp(Vec3) equals Math Lerp bit for bit") {
    const auto grid = Float3Grid(0xC2B2AE35U);
    constexpr std::array<float, 9> ts{0.0F, 1.0F, 0.5F, 0.25F, -0.5F, 1.5F, 1.0e-7F, kInfinity, kNaN};
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& a = grid[i];
        const auto& b = grid[(i * 17 + 1) % grid.size()];
        const float t = ts[i % ts.size()];
        const auto legacy = Legacy::Model::AnimationCpp::Lerp({a[0], a[1], a[2]}, {b[0], b[1], b[2]}, t);
        CompareVec3(tally, legacy.x, legacy.y, legacy.z,
                    Math::Lerp(Math::Vec3{a[0], a[1], a[2]}, Math::Vec3{b[0], b[1], b[2]}, t), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Animation guarded Normalize equals Math NormalizeOrIdentity bit for bit") {
    const auto grid = QuaternionGrid(0x27D4EB2FU);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& q = grid[i];
        const Legacy::Model::Quaternion legacyInput{q[0], q[1], q[2], q[3]};
        CompareQuaternion(tally, Legacy::Model::AnimationCpp::Normalize(legacyInput),
                          Math::NormalizeOrIdentity(ToMath(legacyInput)), i);
    }
    RequireIdentical(tally);
    // Degenerate input becomes the identity on both sides.
    const auto legacyZero = Legacy::Model::AnimationCpp::Normalize({0.0F, 0.0F, 0.0F, 0.0F});
    CHECK(legacyZero.w == 1.0F);
    CHECK(Math::NormalizeOrIdentity({0.0F, 0.0F, 0.0F, 0.0F}).w == 1.0F);
}

TEST_CASE("characterization: Animation Slerp equals Math Slerp bit for bit") {
    const auto grid = QuaternionGrid(0x165667B1U);
    constexpr std::array<float, 8> ts{0.0F, 1.0F, 0.5F, 0.25F, 0.75F, -0.5F, 1.5F, 1.0e-7F};
    Lcg random{0xD3A2646CU};
    BitTally tally;
    std::size_t caseIndex = 0;
    const auto compare = [&](const Float4Input& a, const Float4Input& b, const float t) {
        const Legacy::Model::Quaternion legacyA{a[0], a[1], a[2], a[3]};
        const Legacy::Model::Quaternion legacyB{b[0], b[1], b[2], b[3]};
        CompareQuaternion(tally, Legacy::Model::AnimationCpp::Slerp(legacyA, legacyB, t),
                          Math::Slerp(ToMath(legacyA), ToMath(legacyB), t), caseIndex++);
    };
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& a = grid[i];
        const auto& b = grid[(i * 31 + 7) % grid.size()];
        for (const float t : ts) compare(a, b, t);
        // Nearly parallel (linear fallback above dot 0.9995) and opposite inputs.
        const Float4Input nearby{a[0] + random.Uniform(-1.0e-3F, 1.0e-3F), a[1], a[2] + 1.0e-4F, a[3]};
        compare(a, nearby, ts[i % ts.size()]);
        compare(a, {-a[0], -a[1], -a[2], -a[3]}, ts[(i + 3) % ts.size()]);
        compare(a, a, ts[(i + 5) % ts.size()]);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: AnimationTransfer Normalize equals unguarded Math Normalize bit for bit") {
    const auto grid = QuaternionGrid(0xFD7046C5U);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& q = grid[i];
        const Legacy::Model::Quaternion legacyInput{q[0], q[1], q[2], q[3]};
        CompareQuaternion(tally, Legacy::Model::AnimationTransferCpp::Normalize(legacyInput),
                          Math::Normalize(ToMath(legacyInput)), i);
    }
    RequireIdentical(tally);
    // Unguarded on both sides: the zero quaternion yields NaN.
    const auto legacyZero = Legacy::Model::AnimationTransferCpp::Normalize({0.0F, 0.0F, 0.0F, 0.0F});
    CHECK(std::isnan(legacyZero.w));
    CHECK(std::isnan(Math::Normalize(Math::Quaternion{0.0F, 0.0F, 0.0F, 0.0F}).w));
}

TEST_CASE("characterization: Animation guarded and unguarded Normalize differ only on degenerate input") {
    // Assumes no contraction across the two functions' identical sum of
    // squares. Holds for MSVC, gcc without FMA, and clang -ffp-contract=on;
    // gcc's default -ffp-contract=fast on an FMA target can differ by 1 ulp.
    // Semantic difference kept by name: NormalizeOrIdentity (Animation) and
    // Normalize (AnimationTransfer) agree on every input whose length exceeds
    // 1e-12 and diverge below it.
    const auto grid = QuaternionGrid(0x5BD1E995U);
    for (const auto& q : grid) {
        const Math::Quaternion input{q[0], q[1], q[2], q[3]};
        const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        const Math::Quaternion guarded = Math::NormalizeOrIdentity(input);
        const Math::Quaternion unguarded = Math::Normalize(input);
        if (length <= 1.0e-12F) {
            CHECK(guarded.x == 0.0F);
            CHECK(guarded.w == 1.0F);
        } else {
            CHECK(SameBits(guarded.x, unguarded.x));
            CHECK(SameBits(guarded.y, unguarded.y));
            CHECK(SameBits(guarded.z, unguarded.z));
            CHECK(SameBits(guarded.w, unguarded.w));
        }
    }
    // 1e-13 on every axis: length 2e-13 is below the guard, so the guarded
    // form returns the identity while the unguarded form still divides and
    // returns a different unit rotation.
    const Math::Quaternion tiny{1.0e-13F, 1.0e-13F, 1.0e-13F, 1.0e-13F};
    CHECK(Math::NormalizeOrIdentity(tiny).x == 0.0F);
    CHECK(Math::NormalizeOrIdentity(tiny).w == 1.0F);
    CHECK(Math::Normalize(tiny).x == doctest::Approx(0.5F));
    CHECK(Math::Normalize(tiny).w == doctest::Approx(0.5F));
    // Only an exact zero length makes the unguarded form NaN.
    CHECK(std::isnan(Math::Normalize(Math::Quaternion{0.0F, 0.0F, 0.0F, 0.0F}).w));
}

TEST_CASE("characterization: AnimationTransfer Inverse equals Math Conjugate bit for bit") {
    const auto grid = QuaternionGrid(0x94D049BBU);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& q = grid[i];
        const Legacy::Model::Quaternion legacyInput{q[0], q[1], q[2], q[3]};
        CompareQuaternion(tally, Legacy::Model::AnimationTransferCpp::Inverse(legacyInput),
                          Math::Conjugate(ToMath(legacyInput)), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: AnimationTransfer Product equals Math Normalize(Multiply) bit for bit") {
    const auto grid = QuaternionGrid(0xBF58476DU);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& a = grid[i];
        const auto& b = grid[(i * 19 + 11) % grid.size()];
        const Legacy::Model::Quaternion legacyA{a[0], a[1], a[2], a[3]};
        const Legacy::Model::Quaternion legacyB{b[0], b[1], b[2], b[3]};
        CompareQuaternion(tally, Legacy::Model::AnimationTransferCpp::Product(legacyA, legacyB),
                          Math::Normalize(Math::Multiply(ToMath(legacyA), ToMath(legacyB))), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Model Finite(Vec3) helpers equal Math IsFinite") {
    const auto grid = Float3Grid(0x3C6EF372U);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& v = grid[i];
        const bool math = Math::IsFinite(Math::Vec3{v[0], v[1], v[2]});
        tally.CompareBool(Legacy::Model::AnimationCpp::Finite(Legacy::Model::Vec3{v[0], v[1], v[2]}), math, i);
        tally.CompareBool(Legacy::Model::AnimationTransferCpp::Finite({v[0], v[1], v[2]}), math, i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Animation Finite(Matrix4) equals Math IsFinite(Matrix4)") {
    const auto grid = MatrixGrid(0x7F4A7C15U);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        Legacy::Model::Matrix4 legacy;
        legacy.values = grid[i].values;
        tally.CompareBool(Legacy::Model::AnimationCpp::Finite(legacy), Math::IsFinite(grid[i]), i);
    }
    RequireIdentical(tally);
}

// ===========================================================================
// engine/collision

TEST_CASE("characterization: Collision IsFinite, Length and Normalize equal Math bit for bit") {
    const auto grid = Float3Grid(0xA54FF53AU);
    BitTally finite;
    BitTally length;
    BitTally normalized;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& v = grid[i];
        const Legacy::Collision::Float3 legacyInput{v[0], v[1], v[2]};
        const Math::Vec3 input = ToMath(legacyInput);
        finite.CompareBool(Legacy::Collision::CollisionCpp::IsFinite(legacyInput), Math::IsFinite(input), i);
        length.Compare(Legacy::Collision::CollisionCpp::Length(legacyInput), Math::Length(input), i, 0);
        const auto legacy = Legacy::Collision::CollisionCpp::Normalize(legacyInput);
        CompareVec3(normalized, legacy.x, legacy.y, legacy.z, Math::Normalize(input), i);
    }
    RequireIdentical(finite);
    RequireIdentical(length);
    RequireIdentical(normalized);
}

TEST_CASE("characterization: CapsuleQueries double Vec3 operations equal Math Vec3d bit for bit") {
    using namespace Legacy::Collision::CapsuleQueriesCpp;
    const auto grid = Float3Grid(0x510E527FU);
    BitTally conversion;
    BitTally arithmetic;
    BitTally scalar;
    BitTally finite;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& u = grid[i];
        const auto& w = grid[(i * 23 + 2) % grid.size()];
        const Legacy::Collision::Float3 floatA{u[0], u[1], u[2]};
        const Legacy::Collision::Float3 floatB{w[0], w[1], w[2]};
        const Vec3 a = V(floatA);
        const Vec3 b = V(floatB);
        const Math::Vec3d mathA = Math::ToVec3d(ToMath(floatA));
        const Math::Vec3d mathB = Math::ToVec3d(ToMath(floatB));
        CompareVec3d(conversion, a, mathA, i);
        // A scale that is not a power of two so products round.
        const double factor = static_cast<double>(w[0]) * 1.0000001;
        CompareVec3d(arithmetic, a + b, mathA + mathB, i);
        CompareVec3d(arithmetic, a - b, mathA - mathB, i);
        CompareVec3d(arithmetic, a * factor, mathA * factor, i);
        scalar.Compare(Dot(a, b), Math::Dot(mathA, mathB), i, 0);
        scalar.Compare(Length(a), Math::Length(mathA), i, 1);
        // Narrowing back to float, including doubles outside float range.
        const Vec3 wide = a * 1.0e30;
        const auto narrowed = F(wide);
        CompareVec3(conversion, narrowed.x, narrowed.y, narrowed.z, Math::ToVec3(mathA * 1.0e30), i);
        finite.CompareBool(Finite(floatA), Math::IsFinite(ToMath(floatA)), i);
    }
    RequireIdentical(conversion);
    RequireIdentical(arithmetic);
    RequireIdentical(scalar);
    RequireIdentical(finite);
}

TEST_CASE("characterization: CapsuleQueries Closest equals Math ClosestPoint(Segmentd) bit for bit") {
    using namespace Legacy::Collision::CapsuleQueriesCpp;
    const auto grid = Float3Grid(0x9B05688CU);
    Lcg random{0x1F83D9ABU};
    BitTally tally;
    std::size_t caseIndex = 0;
    const auto compare = [&](const Vec3 p, const Vec3 a, const Vec3 b) {
        CompareVec3d(tally, Closest(p, a, b),
                     Math::ClosestPoint(ToMath(p), Math::Segmentd{ToMath(a), ToMath(b)}), caseIndex++);
    };
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& p = grid[i];
        const auto& a = grid[(i * 29 + 4) % grid.size()];
        const auto& b = grid[(i * 37 + 9) % grid.size()];
        compare(V({p[0], p[1], p[2]}), V({a[0], a[1], a[2]}), V({b[0], b[1], b[2]}));
        // Degenerate segment: both return the start point.
        compare(V({p[0], p[1], p[2]}), V({a[0], a[1], a[2]}), V({a[0], a[1], a[2]}));
    }
    // Short segments far from the origin, where the double path matters.
    for (int i = 0; i < 64; ++i) {
        const Vec3 a{1.0e6 + random.Uniform(-1.0F, 1.0F), 2.0e5, -3.0e6};
        const Vec3 b = a + Vec3{1.0e-3, random.Uniform(-1.0e-3F, 1.0e-3F), 0.0};
        compare({1.0e6, 2.0e5 + random.Uniform(-2.0F, 2.0F), -3.0e6}, a, b);
    }
    RequireIdentical(tally);
    // Degenerate segments return the start endpoint a.
    const Vec3 start{1.0, 2.0, 3.0};
    const Math::Vec3d closest = Math::ClosestPoint(Math::Vec3d{5.0, 5.0, 5.0}, Math::Segmentd{ToMath(start), ToMath(start)});
    CHECK(closest.x == 1.0);
    CHECK(closest.y == 2.0);
    CHECK(closest.z == 3.0);
}

TEST_CASE("characterization: CapsuleQueries Clamp equals Math ClosestPoint(Aabbd) bit for bit") {
    using namespace Legacy::Collision::CapsuleQueriesCpp;
    const auto grid = Float3Grid(0x5BE0CD19U);
    Lcg random{0x6A09E667U};
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& p = grid[i];
        // Ordered finite bounds (std::clamp requires lo <= hi); the point
        // carries the special values.
        float bounds[6]{};
        for (float& bound : bounds) bound = random.Uniform(-50.0F, 50.0F);
        const Vec3 lo{(std::min)(bounds[0], bounds[1]), (std::min)(bounds[2], bounds[3]), (std::min)(bounds[4], bounds[5])};
        const Vec3 hi{(std::max)(bounds[0], bounds[1]), (std::max)(bounds[2], bounds[3]), (std::max)(bounds[4], bounds[5])};
        const Vec3 point = V({p[0], p[1], p[2]});
        CompareVec3d(tally, Clamp(point, lo, hi), Math::ClosestPoint(ToMath(point), Math::Aabbd{ToMath(lo), ToMath(hi)}), i);
    }
    // Zero-size box.
    const Vec3 corner{1.0, -2.0, 3.0};
    CompareVec3d(tally, Clamp({9.0, 9.0, 9.0}, corner, corner),
                 Math::ClosestPoint(Math::Vec3d{9.0, 9.0, 9.0}, Math::Aabbd{ToMath(corner), ToMath(corner)}), grid.size());
    RequireIdentical(tally);
}

// ===========================================================================
// engine/render: PrimitiveMesh

TEST_CASE("characterization: PrimitiveMesh Add, Subtract, Scale and Cross equal Math bit for bit") {
    using namespace Legacy::Render::PrimitiveMeshCpp;
    const auto grid = Float3Grid(0x243F6A88U);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& u = grid[i];
        const auto& w = grid[(i * 41 + 6) % grid.size()];
        const Legacy::Render::Float3 a{u[0], u[1], u[2]};
        const Legacy::Render::Float3 b{w[0], w[1], w[2]};
        const Math::Vec3 mathA = ToMath(a);
        const Math::Vec3 mathB = ToMath(b);
        const float factor = w[1];
        const auto sum = Add(a, b);
        CompareVec3(tally, sum.x, sum.y, sum.z, mathA + mathB, i);
        const auto difference = Subtract(a, b);
        CompareVec3(tally, difference.x, difference.y, difference.z, mathA - mathB, i);
        const auto scaled = Scale(a, factor);
        CompareVec3(tally, scaled.x, scaled.y, scaled.z, mathA * factor, i);
        const auto cross = Cross(a, b);
        CompareVec3(tally, cross.x, cross.y, cross.z, Math::Cross(mathA, mathB), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: PrimitiveMesh IsFinite equals Math IsFinite") {
    const auto grid = Float3Grid(0x85A308D3U);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const auto& v = grid[i];
        tally.CompareBool(Legacy::Render::PrimitiveMeshCpp::IsFinite({v[0], v[1], v[2]}),
                          Math::IsFinite(Math::Vec3{v[0], v[1], v[2]}), i);
    }
    RequireIdentical(tally);
}

// Expected drift: PrimitiveMesh Length uses std::hypot(x, y, z), Math::Length
// uses std::sqrt(x*x + y*y + z*z). Measured on Apple clang 21 / libc++
// (x86_64): 0 ulp over the normal-range grid below, because libc++'s
// three-argument hypot evaluates the same sqrt expression whenever the largest
// |component| lies in [2^-64, 2^64]. libstdc++ and MSVC scale by the largest
// component instead (a * sqrt((x/a)^2 + ...)); emulating that formula over
// the same grid measured 3 ulp. The bound is the larger measurement.
constexpr std::int64_t kHypotDriftUlpBound = 3;

TEST_CASE("characterization: PrimitiveMesh hypot Length drifts from Math sqrt Length within a measured bound") {
    Lcg random{0x13198A2EU};
    std::int64_t maximumUlp = 0;
    double maximumRelative = 0.0;
    for (int i = 0; i < 100000; ++i) {
        // Log-uniform magnitudes in [5e-7, 1.5e6] with random signs: every
        // square stays a normal float.
        float components[3]{};
        for (float& component : components) {
            const float mantissa = random.Uniform(0.5F, 1.5F);
            const float exponent = static_cast<float>(static_cast<int>(random.Next() % 13U)) - 6.0F;
            component = mantissa * std::pow(10.0F, exponent) * ((random.Next() & 1U) != 0U ? -1.0F : 1.0F);
        }
        const float legacy = Legacy::Render::PrimitiveMeshCpp::Length({components[0], components[1], components[2]});
        const float math = Math::Length(Math::Vec3{components[0], components[1], components[2]});
        maximumUlp = (std::max)(maximumUlp, UlpDistance(legacy, math));
        maximumRelative = (std::max)(maximumRelative,
                                     std::abs(static_cast<double>(legacy) - static_cast<double>(math)) /
                                         static_cast<double>(legacy));
    }
    INFO("max ulp " << maximumUlp << ", max relative error " << maximumRelative);
    CHECK(maximumUlp <= kHypotDriftUlpBound);
    CHECK(maximumRelative <= 4.0e-7);
}

// Replacing PrimitiveMesh's reciprocal normalization with Math::Normalize
// (divide each component by the sqrt length) is a drift, not an identity.
// Measured on Apple clang 21 / libc++ x86_64.
constexpr std::int64_t kReciprocalNormalizeDriftUlpBound = 2;

TEST_CASE("characterization: PrimitiveMesh reciprocal normalization drifts from Math Normalize within a measured bound") {
    Lcg random{0x0A2D1E5FU};
    std::int64_t maximumUlp = 0;
    std::size_t differing = 0;
    constexpr int kCount = 100000;
    for (int i = 0; i < kCount; ++i) {
        float components[3]{};
        for (float& component : components) {
            const float mantissa = random.Uniform(0.5F, 1.5F);
            const float exponent = static_cast<float>(static_cast<int>(random.Next() % 13U)) - 6.0F;
            component = mantissa * std::pow(10.0F, exponent) * ((random.Next() & 1U) != 0U ? -1.0F : 1.0F);
        }
        const auto legacy =
            Legacy::Render::PrimitiveMeshCpp::ScaleByReciprocalLength({components[0], components[1], components[2]});
        const Math::Vec3 math = Math::Normalize(Math::Vec3{components[0], components[1], components[2]});
        const std::int64_t ulp = (std::max)({UlpDistance(legacy.x, math.x), UlpDistance(legacy.y, math.y),
                                             UlpDistance(legacy.z, math.z)});
        maximumUlp = (std::max)(maximumUlp, ulp);
        if (ulp != 0) ++differing;
    }
    INFO("max ulp " << maximumUlp << ", differing " << differing << " of " << kCount);
    CHECK(maximumUlp <= kReciprocalNormalizeDriftUlpBound);
}

TEST_CASE("characterization: PrimitiveMesh hypot Length and Math sqrt Length differ at overflow and underflow") {
    // Semantic difference: hypot avoids intermediate overflow/underflow, the
    // sqrt of the sum of squares does not (documented on Math::Length).
    const Legacy::Render::Float3 huge{1.0e20F, 1.0e20F, 1.0e20F};
    const float legacyHuge = Legacy::Render::PrimitiveMeshCpp::Length(huge);
    CHECK(std::isfinite(legacyHuge));
    CHECK(legacyHuge == doctest::Approx(1.7320508e20F).epsilon(1.0e-6));
    CHECK(std::isinf(Math::Length(ToMath(huge))));

    const Legacy::Render::Float3 tiny{1.0e-25F, 1.0e-25F, 1.0e-25F};
    const float legacyTiny = Legacy::Render::PrimitiveMeshCpp::Length(tiny);
    CHECK(legacyTiny > 0.0F);
    CHECK(static_cast<double>(legacyTiny) == doctest::Approx(1.7320508e-25).epsilon(1.0e-6));
    CHECK(Math::Length(ToMath(tiny)) == 0.0F);

    // A single overflowing component.
    CHECK(std::isfinite(Legacy::Render::PrimitiveMeshCpp::Length({3.0e20F, 0.0F, 0.0F})));
    CHECK(std::isinf(Math::Length(Math::Vec3{3.0e20F, 0.0F, 0.0F})));
}

// ===========================================================================
// engine/render: Renderer matrices (row-major, row vectors) against Math
// (column-major, column vectors). The 16 floats in memory order must match.

TEST_CASE("characterization: Renderer basic matrices equal Math constructors bit for bit") {
    using namespace Legacy::Render::RendererCpp;
    BitTally tally;
    CompareMatrix(tally, Flatten(Identity()), Math::Matrix4{}, 0);
    const auto vectors = Float3Grid(0xA4093822U);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        const auto& v = vectors[i];
        const Legacy::Render::Float3 value{v[0], v[1], v[2]};
        CompareMatrix(tally, Flatten(Translation(value)), Math::MakeTranslation(ToMath(value)), i);
        CompareMatrix(tally, Flatten(Scale(value)), Math::MakeScale(ToMath(value)), i);
    }
    Lcg random{0x299F31D0U};
    std::vector<float> angles(kAngles.begin(), kAngles.end());
    angles.insert(angles.end(), kSpecialFloats.begin(), kSpecialFloats.end());
    for (int i = 0; i < 256; ++i) angles.push_back(random.Uniform(-20.0F, 20.0F));
    for (std::size_t i = 0; i < angles.size(); ++i) {
        CompareMatrix(tally, Flatten(RotationX(angles[i])), Math::MakeRotationX(angles[i]), i);
        CompareMatrix(tally, Flatten(RotationY(angles[i])), Math::MakeRotationY(angles[i]), i);
        CompareMatrix(tally, Flatten(RotationZ(angles[i])), Math::MakeRotationZ(angles[i]), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Renderer Multiply(A, B) equals Math Multiply(B, A) bit for bit") {
    const auto grid = MatrixGrid(0x082EFA98U);
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        const Math::Matrix4& a = grid[i];
        const Math::Matrix4& b = grid[(i * 43 + 5) % grid.size()];
        CompareMatrix(tally, Flatten(Legacy::Render::RendererCpp::Multiply(Unflatten(a), Unflatten(b))),
                      Math::Multiply(b, a), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Renderer WorldMatrix equals Math ComposeEulerXYZ bit for bit") {
    const auto grid = TransformGrid();
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        CompareMatrix(tally, Flatten(Legacy::Render::RendererCpp::WorldMatrix(grid[i])), MathWorldMatrix(grid[i]), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Renderer ViewMatrix equals the mirrored Math chain bit for bit") {
    const auto grid = CameraGrid();
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        CompareMatrix(tally, Flatten(Legacy::Render::RendererCpp::ViewMatrix(grid[i])), MathViewMatrix(grid[i]), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Renderer ProjectionMatrix equals Math MakePerspective bit for bit") {
    const auto grid = CameraGrid();
    BitTally tally;
    std::size_t caseIndex = 0;
    for (const auto& camera : grid) {
        for (const float aspectRatio : kAspectRatios) {
            CompareMatrix(tally, Flatten(Legacy::Render::RendererCpp::ProjectionMatrix(camera, aspectRatio)),
                          MathProjectionMatrix(camera, aspectRatio), caseIndex++);
        }
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Renderer SpriteWorldMatrix equals the mirrored Math chain bit for bit") {
    const auto grid = SpriteGrid();
    BitTally tally;
    for (std::size_t i = 0; i < grid.size(); ++i) {
        CompareMatrix(tally, Flatten(Legacy::Render::RendererCpp::SpriteWorldMatrix(grid[i])),
                      MathSpriteWorldMatrix(grid[i]), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Renderer PixelProjection equals Math MakeOrthographicPixels bit for bit") {
    BitTally tally;
    for (std::size_t i = 0; i < kPixelSizes.size(); ++i) {
        const float width = kPixelSizes[i][0];
        const float height = kPixelSizes[i][1];
        CompareMatrix(tally, Flatten(Legacy::Render::RendererCpp::PixelProjection(width, height)),
                      Math::MakeOrthographicPixels(width, height), i);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: Renderer world-view-projection chain equals Math with the same association") {
    // Renderer.cpp:308-320 computes World * (View * Projection). The mirror
    // keeps the association: Multiply(Multiply(Projection, View), World).
    const auto transforms = TransformGrid();
    const auto cameras = CameraGrid();
    BitTally tally;
    for (std::size_t i = 0; i < transforms.size(); ++i) {
        const auto& camera = cameras[(i * 7) % cameras.size()];
        const float aspectRatio = kAspectRatios[i % kAspectRatios.size()];
        using namespace Legacy::Render::RendererCpp;
        const auto legacy = Multiply(WorldMatrix(transforms[i]),
                                     Multiply(ViewMatrix(camera), ProjectionMatrix(camera, aspectRatio)));
        const Math::Matrix4 math = Math::Multiply(
            Math::Multiply(MathProjectionMatrix(camera, aspectRatio), MathViewMatrix(camera)),
            MathWorldMatrix(transforms[i]));
        CompareMatrix(tally, Flatten(legacy), math, i);
    }
    RequireIdentical(tally);
}

// Largest |kept - reassociated| divided by the largest |element| of the
// matrix. Measured 2.72e-7 (540 differing elements over the grid) on Apple
// clang 21 x86_64 at -O0 and -O2; the bound leaves headroom for targets that
// contract multiply-adds into FMA.
constexpr double kReassociationScaledErrorBound = 4.0e-7;

TEST_CASE("characterization: re-associating the world-view-projection chain drifts") {
    // Multiply(Projection, Multiply(View, World)) is the same product with the
    // other association; float products are not associative, so it is not
    // bit-identical. B4b must keep the association above to stay bit-identical.
    const auto transforms = TransformGrid();
    const auto cameras = CameraGrid();
    std::size_t differing = 0;
    double maximumScaledError = 0.0;
    for (std::size_t i = 0; i < transforms.size(); ++i) {
        const auto& camera = cameras[(i * 7) % cameras.size()];
        const float aspectRatio = kAspectRatios[i % kAspectRatios.size()];
        const Math::Matrix4 kept = Math::Multiply(
            Math::Multiply(MathProjectionMatrix(camera, aspectRatio), MathViewMatrix(camera)),
            MathWorldMatrix(transforms[i]));
        const Math::Matrix4 reassociated = Math::Multiply(
            MathProjectionMatrix(camera, aspectRatio),
            Math::Multiply(MathViewMatrix(camera), MathWorldMatrix(transforms[i])));
        if (!Math::IsFinite(kept) || !Math::IsFinite(reassociated)) continue;
        double magnitude = 0.0;
        for (const float value : kept.values) magnitude = (std::max)(magnitude, std::abs(static_cast<double>(value)));
        for (std::size_t k = 0; k < 16; ++k) {
            if (!SameBits(kept.values[k], reassociated.values[k])) ++differing;
            const double error = std::abs(static_cast<double>(kept.values[k]) -
                                          static_cast<double>(reassociated.values[k]));
            maximumScaledError = (std::max)(maximumScaledError, error / magnitude);
        }
    }
    INFO("differing elements " << differing << ", max error / max |element| " << maximumScaledError);
    CHECK(differing > 0);
    CHECK(maximumScaledError <= kReassociationScaledErrorBound);
}

TEST_CASE("characterization: Renderer sprite chain equals Math with the same association") {
    // Renderer.cpp:327-334 computes SpriteWorld * PixelProjection.
    const auto sprites = SpriteGrid();
    BitTally tally;
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        const auto& size = kPixelSizes[i % 5];
        using namespace Legacy::Render::RendererCpp;
        const auto legacy = Multiply(SpriteWorldMatrix(sprites[i]), PixelProjection(size[0], size[1]));
        const Math::Matrix4 math =
            Math::Multiply(Math::MakeOrthographicPixels(size[0], size[1]), MathSpriteWorldMatrix(sprites[i]));
        CompareMatrix(tally, Flatten(legacy), math, i);
    }
    RequireIdentical(tally);
}

// ===========================================================================
// engine/render and engine/ui: finite checks

TEST_CASE("characterization: Render and Ui finite checks equal Math IsFinite") {
    BitTally tally;
    std::size_t caseIndex = 0;
    for (const float x : kSpecialFloats) {
        for (const float y : kSpecialFloats) {
            const bool vec2 = Math::IsFinite(Math::Vec2{x, y});
            tally.CompareBool(Legacy::Render::RenderQueueCpp::IsFinite(Legacy::Render::Float2{x, y}), vec2, caseIndex);
            tally.CompareBool(Legacy::Ui::UiValidationCpp::IsFinite(Legacy::Ui::UiFloat2{x, y}), vec2, caseIndex);
            for (const float z : kSpecialFloats) {
                const Legacy::Render::Float3 value{x, y, z};
                const bool vec3 = Math::IsFinite(ToMath(value));
                tally.CompareBool(Legacy::Render::RenderQueueCpp::IsFinite(value), vec3, caseIndex);
                tally.CompareBool(Legacy::Render::RendererCpp::IsFinite(value), vec3, caseIndex);
                tally.CompareBool(Legacy::Render::SdlGpuRenderDeviceCpp::IsFinite(value), vec3, caseIndex);
                for (const float w : kSpecialFloats) {
                    tally.CompareBool(Legacy::Render::RenderQueueCpp::IsFinite(Legacy::Render::Rect{x, y, z, w}),
                                      Math::IsFinite(Math::Rect{x, y, z, w}), caseIndex);
                }
                ++caseIndex;
            }
        }
    }
    RequireIdentical(tally);
}

// ===========================================================================
// sRGB transfer functions

namespace {

// Every 65537th bit pattern (all signs, subnormals, infinities and NaNs), a
// stride through [0, 1], all 256 byte values as UiColor forms them, and the
// thresholds' neighbours.
[[nodiscard]] std::vector<float> SrgbSweep() {
    std::vector<float> sweep;
    for (std::uint64_t bits = 0; bits <= 0xFFFFFFFFULL; bits += 65537ULL) {
        sweep.push_back(std::bit_cast<float>(static_cast<std::uint32_t>(bits)));
    }
    const std::uint32_t one = std::bit_cast<std::uint32_t>(1.0F);
    for (std::uint32_t bits = 0; bits <= one; bits += 4099U) sweep.push_back(std::bit_cast<float>(bits));
    for (unsigned channel = 0; channel < 256U; ++channel) {
        sweep.push_back(static_cast<float>(channel) * Legacy::Ui::UiColorCpp::kByteScale);
        sweep.push_back(static_cast<float>(channel) * (1.0F / 255.0F));
    }
    for (const float threshold : {0.04045F, 0.0031308F, 0.0F, 1.0F}) {
        sweep.push_back(std::nextafter(threshold, -kInfinity));
        sweep.push_back(threshold);
        sweep.push_back(std::nextafter(threshold, kInfinity));
    }
    Lcg random{0xEC4E6C89U};
    for (int i = 0; i < 4096; ++i) sweep.push_back(random.Uniform(-0.5F, 1.5F));
    sweep.insert(sweep.end(), kSpecialFloats.begin(), kSpecialFloats.end());
    return sweep;
}

} // namespace

TEST_CASE("characterization: Render sRGB components equal Math DecodeSrgb and EncodeSrgb bit for bit") {
    const auto sweep = SrgbSweep();
    BitTally decode;
    BitTally encode;
    for (std::size_t i = 0; i < sweep.size(); ++i) {
        decode.Compare(Legacy::Render::ColorTransformCpp::DecodeSrgbComponent(sweep[i]), Math::DecodeSrgb(sweep[i]), i, 0);
        encode.Compare(Legacy::Render::ColorTransformCpp::EncodeSrgbComponent(sweep[i]), Math::EncodeSrgb(sweep[i]), i, 0);
    }
    RequireIdentical(decode);
    RequireIdentical(encode);
}

TEST_CASE("characterization: UiColor sRGB decode equals Math DecodeSrgb on every byte value") {
    // UiColor.cpp:78-80 decodes channel * kByteScale for channel 0..255, all
    // of which lie in [0, 1] where the missing clamp makes no difference.
    BitTally tally;
    for (unsigned channel = 0; channel < 256U; ++channel) {
        const float encoded = static_cast<float>(channel) * Legacy::Ui::UiColorCpp::kByteScale;
        CHECK(encoded >= 0.0F);
        CHECK(encoded <= 1.0F);
        tally.Compare(Legacy::Ui::UiColorCpp::SrgbToLinear(encoded), Math::DecodeSrgb(encoded), channel, 0);
    }
    RequireIdentical(tally);
}

TEST_CASE("characterization: UiColor unclamped sRGB decode differs from Math DecodeSrgb outside [0, 1]") {
    // Semantic difference: SrgbToLinear does not clamp; DecodeSrgb clamps the
    // input to [0, 1] like the Render component it also replaces.
    const auto sweep = SrgbSweep();
    BitTally inside;
    for (std::size_t i = 0; i < sweep.size(); ++i) {
        const float value = sweep[i];
        const float legacy = Legacy::Ui::UiColorCpp::SrgbToLinear(value);
        const float math = Math::DecodeSrgb(value);
        if ((value >= 0.0F && value <= 1.0F) || std::isnan(value)) {
            inside.Compare(legacy, math, i, 0);
        } else {
            CHECK(math == Math::DecodeSrgb(value < 0.0F ? 0.0F : 1.0F));
        }
    }
    RequireIdentical(inside);
    // Negative input extrapolates the linear segment, above-one input the curve.
    CHECK(Legacy::Ui::UiColorCpp::SrgbToLinear(-0.5F) == -0.5F / 12.92F);
    CHECK(Math::DecodeSrgb(-0.5F) == 0.0F);
    CHECK(Legacy::Ui::UiColorCpp::SrgbToLinear(2.0F) > 1.0F);
    CHECK(Math::DecodeSrgb(2.0F) == 1.0F);
    CHECK(std::isinf(Legacy::Ui::UiColorCpp::SrgbToLinear(kInfinity)));
    CHECK(Math::DecodeSrgb(kInfinity) == 1.0F);
}

TEST_CASE("characterization: UiColor sRGB encode equals Math EncodeSrgb bit for bit") {
    const auto sweep = SrgbSweep();
    BitTally tally;
    for (std::size_t i = 0; i < sweep.size(); ++i) {
        tally.Compare(Legacy::Ui::UiColorCpp::LinearToSrgb(sweep[i]), Math::EncodeSrgb(sweep[i]), i, 0);
    }
    RequireIdentical(tally);
}

// ===========================================================================
// engine/ui: rectangles

TEST_CASE("characterization: UiRuntime IntersectRect equals Math Intersection bit for bit") {
    Lcg random{0x7FEB352DU};
    std::vector<std::array<float, 4>> rects;
    for (int i = 0; i < 256; ++i) {
        rects.push_back({random.Uniform(-500.0F, 500.0F), random.Uniform(-500.0F, 500.0F),
                         random.Uniform(-10.0F, 600.0F), random.Uniform(-10.0F, 600.0F)});
    }
    // Touching, nested, identical and zero-size rectangles.
    rects.push_back({0.0F, 0.0F, 10.0F, 10.0F});
    rects.push_back({10.0F, 0.0F, 10.0F, 10.0F});
    rects.push_back({2.0F, 2.0F, 3.0F, 3.0F});
    rects.push_back({5.0F, 5.0F, 0.0F, 0.0F});
    for (const float special : kSpecialFloats) {
        for (std::size_t component = 0; component < 4; ++component) {
            std::array<float, 4> rect{random.Uniform(-50.0F, 50.0F), random.Uniform(-50.0F, 50.0F),
                                      random.Uniform(0.0F, 100.0F), random.Uniform(0.0F, 100.0F)};
            rect[component] = special;
            rects.push_back(rect);
        }
    }
    BitTally tally;
    std::size_t caseIndex = 0;
    for (std::size_t i = 0; i < rects.size(); ++i) {
        for (std::size_t j = 0; j < rects.size(); j += 7) {
            const auto& a = rects[i];
            const auto& b = rects[j];
            const auto legacy = Legacy::Ui::UiRuntimeCpp::IntersectRect({a[0], a[1], a[2], a[3]}, {b[0], b[1], b[2], b[3]});
            const Math::Rect math = Math::Intersection({a[0], a[1], a[2], a[3]}, {b[0], b[1], b[2], b[3]});
            tally.Compare(legacy.x, math.x, caseIndex, 0);
            tally.Compare(legacy.y, math.y, caseIndex, 1);
            tally.Compare(legacy.width, math.width, caseIndex, 2);
            tally.Compare(legacy.height, math.height, caseIndex, 3);
            ++caseIndex;
        }
    }
    RequireIdentical(tally);
}
