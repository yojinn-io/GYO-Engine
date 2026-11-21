#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <type_traits>

#include "engine/math/linear/Matrix3.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec3.hpp"

namespace Engine::Math {

// Column-major storage (values[column * 4 + row]) and column vectors:
// p' = M * p, translation in values[12..14], Multiply(parent, local) applies
// local first. The default is the identity; Zero() is the all-zero matrix
// (note Matrix4{{}} is also all zeros, unlike Matrix4{}).
//
// GPU uniforms declare row_major float4x4 and compute mul(float4(p, 1), M);
// those 16 floats equal these values in order, so uploads copy without a
// transpose.
struct Matrix4 final {
    std::array<float, 16> values{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

// GPU uploads copy the 16 floats directly; the array must start the struct.
static_assert(sizeof(Matrix4) == 64 && alignof(Matrix4) == 4 && offsetof(Matrix4, values) == 0);
static_assert(std::is_trivially_copyable_v<Matrix4> && std::is_standard_layout_v<Matrix4>);
static_assert(std::is_aggregate_v<Matrix4>);

[[nodiscard]] inline Matrix4 Zero() noexcept {
    Matrix4 result;
    result.values.fill(0);
    return result;
}

// Multiply(a, b) applies b first, then a.
[[nodiscard]] inline Matrix4 Multiply(const Matrix4& a, const Matrix4& b) noexcept {
    Matrix4 result;
    result.values.fill(0);
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t k = 0; k < 4; ++k) {
                result.values[column * 4 + row] += a.values[k * 4 + row] * b.values[column * 4 + k];
            }
        }
    }
    return result;
}

// Applies translation (w = 1). Non-template so TransformPoint(m, {}) works.
[[nodiscard]] inline Vec3 TransformPoint(const Matrix4& matrix, const Vec3 point) noexcept {
    const auto& m = matrix.values;
    return {m[0] * point.x + m[4] * point.y + m[8] * point.z + m[12],
            m[1] * point.x + m[5] * point.y + m[9] * point.z + m[13],
            m[2] * point.x + m[6] * point.y + m[10] * point.z + m[14]};
}

// Ignores translation (w = 0).
[[nodiscard]] inline Vec3 TransformVector(const Matrix4& matrix, const Vec3 vector) noexcept {
    const auto& m = matrix.values;
    return {m[0] * vector.x + m[4] * vector.y + m[8] * vector.z,
            m[1] * vector.x + m[5] * vector.y + m[9] * vector.z,
            m[2] * vector.x + m[6] * vector.y + m[10] * vector.z};
}

[[nodiscard]] inline Matrix4 MakeTranslation(const Vec3 translation) noexcept {
    Matrix4 result;
    result.values[12] = translation.x;
    result.values[13] = translation.y;
    result.values[14] = translation.z;
    return result;
}

[[nodiscard]] inline Matrix4 MakeScale(const Vec3 scale) noexcept {
    Matrix4 result;
    result.values[0] = scale.x;
    result.values[5] = scale.y;
    result.values[10] = scale.z;
    return result;
}

// MakeRotationX(a) * (0, 0, 1) = (0, -sin a, cos a).
[[nodiscard]] inline Matrix4 MakeRotationX(const float radians) noexcept {
    Matrix4 result;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[5] = cosine;
    result.values[6] = sine;
    result.values[9] = -sine;
    result.values[10] = cosine;
    return result;
}

// MakeRotationY(a) * (0, 0, 1) = (sin a, 0, cos a): positive yaw turns +Z toward +X.
[[nodiscard]] inline Matrix4 MakeRotationY(const float radians) noexcept {
    Matrix4 result;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0] = cosine;
    result.values[2] = -sine;
    result.values[8] = sine;
    result.values[10] = cosine;
    return result;
}

// MakeRotationZ(a) * (1, 0, 0) = (cos a, sin a, 0).
[[nodiscard]] inline Matrix4 MakeRotationZ(const float radians) noexcept {
    Matrix4 result;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0] = cosine;
    result.values[1] = sine;
    result.values[4] = -sine;
    result.values[5] = cosine;
    return result;
}

// Translation * Rotation(q) * Scale. The quaternion need not be unit length;
// a zero quaternion applies no rotation (identity rotation block; scale and
// translation still apply) instead of producing NaN.
[[nodiscard]] inline Matrix4 ComposeTRS(const Vec3 translation, const Quaternion& q, const Vec3 scale) noexcept {
    const float lengthSquared = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    const float s = lengthSquared > 0 ? 2.0F / lengthSquared : 0.0F;
    const float xx = q.x * q.x * s, xy = q.x * q.y * s, xz = q.x * q.z * s;
    const float yy = q.y * q.y * s, yz = q.y * q.z * s, zz = q.z * q.z * s;
    const float wx = q.w * q.x * s, wy = q.w * q.y * s, wz = q.w * q.z * s;
    Matrix4 result;
    result.values = {
        (1 - yy - zz) * scale.x, (xy + wz) * scale.x, (xz - wy) * scale.x, 0,
        (xy - wz) * scale.y, (1 - xx - zz) * scale.y, (yz + wx) * scale.y, 0,
        (xz + wy) * scale.z, (yz - wx) * scale.z, (1 - xx - yy) * scale.z, 0,
        translation.x, translation.y, translation.z, 1};
    return result;
}

[[nodiscard]] inline Matrix4 MakeRotation(const Quaternion& q) noexcept {
    return ComposeTRS({}, q, {1.0F, 1.0F, 1.0F});
}

// GYO Euler convention: scale, then rotate about X, then Y, then Z (radians),
// then translate.
[[nodiscard]] inline Matrix4 ComposeEulerXYZ(
    const Vec3 translation, const Vec3 rotationRadians, const Vec3 scale) noexcept {
    Matrix4 result = MakeScale(scale);
    result = Multiply(MakeRotationX(rotationRadians.x), result);
    result = Multiply(MakeRotationY(rotationRadians.y), result);
    result = Multiply(MakeRotationZ(rotationRadians.z), result);
    return Multiply(MakeTranslation(translation), result);
}

// Left-handed perspective projection with clip depth in [0, 1]: near maps to
// 0, far to 1, and clip w equals view-space z.
[[nodiscard]] inline Matrix4 MakePerspective(
    const float verticalFieldOfViewRadians,
    const float aspectRatio,
    const float nearClip,
    const float farClip) noexcept {
    Matrix4 result = Zero();
    const float yScale = 1.0F / std::tan(verticalFieldOfViewRadians * 0.5F);
    const float xScale = yScale / aspectRatio;
    const float depthRange = farClip - nearClip;
    result.values[0] = xScale;
    result.values[5] = yScale;
    result.values[10] = farClip / depthRange;
    result.values[11] = 1.0F;
    result.values[14] = -(nearClip * farClip) / depthRange;
    return result;
}

// Maps pixel space (origin top-left, +Y down, width x height) to clip space
// x, y in [-1, 1] with +Y up; z passes through.
[[nodiscard]] inline Matrix4 MakeOrthographicPixels(const float width, const float height) noexcept {
    Matrix4 result = Zero();
    result.values[0] = 2.0F / width;
    result.values[5] = -2.0F / height;
    result.values[10] = 1.0F;
    result.values[12] = -1.0F;
    result.values[13] = 1.0F;
    result.values[15] = 1.0F;
    return result;
}

[[nodiscard]] inline Matrix4 Transpose(const Matrix4& matrix) noexcept {
    Matrix4 result;
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            result.values[column * 4 + row] = matrix.values[row * 4 + column];
        }
    }
    return result;
}

[[nodiscard]] inline float Determinant(const Matrix4& matrix) noexcept {
    const auto& m = matrix.values;
    const float a0 = m[0] * m[5] - m[1] * m[4];
    const float a1 = m[0] * m[6] - m[2] * m[4];
    const float a2 = m[0] * m[7] - m[3] * m[4];
    const float a3 = m[1] * m[6] - m[2] * m[5];
    const float a4 = m[1] * m[7] - m[3] * m[5];
    const float a5 = m[2] * m[7] - m[3] * m[6];
    const float b0 = m[8] * m[13] - m[9] * m[12];
    const float b1 = m[8] * m[14] - m[10] * m[12];
    const float b2 = m[8] * m[15] - m[11] * m[12];
    const float b3 = m[9] * m[14] - m[10] * m[13];
    const float b4 = m[9] * m[15] - m[11] * m[13];
    const float b5 = m[10] * m[15] - m[11] * m[14];
    return a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
}

// General inverse by cofactors. nullopt when the determinant is exactly zero
// or the result is not finite.
[[nodiscard]] inline std::optional<Matrix4> Inverse(const Matrix4& matrix) noexcept {
    const auto& m = matrix.values;
    const float a0 = m[0] * m[5] - m[1] * m[4];
    const float a1 = m[0] * m[6] - m[2] * m[4];
    const float a2 = m[0] * m[7] - m[3] * m[4];
    const float a3 = m[1] * m[6] - m[2] * m[5];
    const float a4 = m[1] * m[7] - m[3] * m[5];
    const float a5 = m[2] * m[7] - m[3] * m[6];
    const float b0 = m[8] * m[13] - m[9] * m[12];
    const float b1 = m[8] * m[14] - m[10] * m[12];
    const float b2 = m[8] * m[15] - m[11] * m[12];
    const float b3 = m[9] * m[14] - m[10] * m[13];
    const float b4 = m[9] * m[15] - m[11] * m[13];
    const float b5 = m[10] * m[15] - m[11] * m[14];
    const float determinant = a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
    if (determinant == 0.0F || !std::isfinite(determinant)) return std::nullopt;
    const float inverse = 1.0F / determinant;
    Matrix4 result{{
        (m[5] * b5 - m[6] * b4 + m[7] * b3) * inverse,
        (-m[1] * b5 + m[2] * b4 - m[3] * b3) * inverse,
        (m[13] * a5 - m[14] * a4 + m[15] * a3) * inverse,
        (-m[9] * a5 + m[10] * a4 - m[11] * a3) * inverse,
        (-m[4] * b5 + m[6] * b2 - m[7] * b1) * inverse,
        (m[0] * b5 - m[2] * b2 + m[3] * b1) * inverse,
        (-m[12] * a5 + m[14] * a2 - m[15] * a1) * inverse,
        (m[8] * a5 - m[10] * a2 + m[11] * a1) * inverse,
        (m[4] * b4 - m[5] * b2 + m[7] * b0) * inverse,
        (-m[0] * b4 + m[1] * b2 - m[3] * b0) * inverse,
        (m[12] * a4 - m[13] * a2 + m[15] * a0) * inverse,
        (-m[8] * a4 + m[9] * a2 - m[11] * a0) * inverse,
        (-m[4] * b3 + m[5] * b1 - m[6] * b0) * inverse,
        (m[0] * b3 - m[1] * b1 + m[2] * b0) * inverse,
        (-m[12] * a3 + m[13] * a1 - m[14] * a0) * inverse,
        (m[8] * a3 - m[9] * a1 + m[10] * a0) * inverse,
    }};
    if (!std::all_of(result.values.begin(), result.values.end(), [](float v) { return std::isfinite(v); })) {
        return std::nullopt;
    }
    return result;
}

// Upper-left 3x3 block (rotation and scale, no translation).
[[nodiscard]] inline Matrix3 ToMatrix3(const Matrix4& matrix) noexcept {
    const auto& m = matrix.values;
    return {{m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]}};
}

// Embeds a 3x3 block with no translation.
[[nodiscard]] inline Matrix4 ToMatrix4(const Matrix3& matrix) noexcept {
    const auto& m = matrix.values;
    return {{m[0], m[1], m[2], 0, m[3], m[4], m[5], 0, m[6], m[7], m[8], 0, 0, 0, 0, 1}};
}

[[nodiscard]] inline bool IsFinite(const Matrix4& matrix) noexcept {
    return std::all_of(matrix.values.begin(), matrix.values.end(), [](float v) { return std::isfinite(v); });
}

} // namespace Engine::Math
