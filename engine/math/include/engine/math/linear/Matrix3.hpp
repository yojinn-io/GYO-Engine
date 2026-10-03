#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <type_traits>

#include "engine/math/linear/Vec3.hpp"

namespace Engine::Math {

// Same convention as Matrix4: column-major storage (values[column * 3 + row])
// and column vectors (p' = M * p). The default is the identity.
struct Matrix3 final {
    std::array<float, 9> values{1, 0, 0, 0, 1, 0, 0, 0, 1};
};

static_assert(sizeof(Matrix3) == 36 && alignof(Matrix3) == 4 && offsetof(Matrix3, values) == 0);
static_assert(std::is_trivially_copyable_v<Matrix3> && std::is_standard_layout_v<Matrix3>);
static_assert(std::is_aggregate_v<Matrix3>);

// Multiply(a, b) applies b first, then a.
[[nodiscard]] inline Matrix3 Multiply(const Matrix3& a, const Matrix3& b) noexcept {
    Matrix3 result;
    result.values.fill(0);
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t k = 0; k < 3; ++k) {
                result.values[column * 3 + row] += a.values[k * 3 + row] * b.values[column * 3 + k];
            }
        }
    }
    return result;
}

[[nodiscard]] inline Vec3 Transform(const Matrix3& matrix, const Vec3 v) noexcept {
    const auto& m = matrix.values;
    return {m[0] * v.x + m[3] * v.y + m[6] * v.z,
            m[1] * v.x + m[4] * v.y + m[7] * v.z,
            m[2] * v.x + m[5] * v.y + m[8] * v.z};
}

[[nodiscard]] inline Matrix3 Transpose(const Matrix3& matrix) noexcept {
    const auto& m = matrix.values;
    return {{m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]}};
}

[[nodiscard]] inline float Determinant(const Matrix3& matrix) noexcept {
    const auto& m = matrix.values;
    return m[0] * (m[4] * m[8] - m[7] * m[5]) -
           m[3] * (m[1] * m[8] - m[7] * m[2]) +
           m[6] * (m[1] * m[5] - m[4] * m[2]);
}

// nullopt when the determinant is exactly zero or the result is not finite.
[[nodiscard]] inline std::optional<Matrix3> Inverse(const Matrix3& matrix) noexcept {
    const auto& m = matrix.values;
    const float determinant = Determinant(matrix);
    if (determinant == 0.0F || !std::isfinite(determinant)) return std::nullopt;
    const float inverse = 1.0F / determinant;
    Matrix3 result{{
        (m[4] * m[8] - m[7] * m[5]) * inverse,
        (m[7] * m[2] - m[1] * m[8]) * inverse,
        (m[1] * m[5] - m[4] * m[2]) * inverse,
        (m[6] * m[5] - m[3] * m[8]) * inverse,
        (m[0] * m[8] - m[6] * m[2]) * inverse,
        (m[3] * m[2] - m[0] * m[5]) * inverse,
        (m[3] * m[7] - m[6] * m[4]) * inverse,
        (m[6] * m[1] - m[0] * m[7]) * inverse,
        (m[0] * m[4] - m[3] * m[1]) * inverse,
    }};
    if (!std::all_of(result.values.begin(), result.values.end(), [](float v) { return std::isfinite(v); })) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] inline bool IsFinite(const Matrix3& matrix) noexcept {
    return std::all_of(matrix.values.begin(), matrix.values.end(), [](float v) { return std::isfinite(v); });
}

} // namespace Engine::Math
