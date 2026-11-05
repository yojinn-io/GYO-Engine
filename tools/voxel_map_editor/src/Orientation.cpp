#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

constexpr Matrix3i Multiply(const Matrix3i& a, const Matrix3i& b) {
    Matrix3i result{};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            for (int k = 0; k < 3; ++k) result[row][column] += a[row][k] * b[k][column];
    return result;
}

// Takes +Y to the up direction of o / 4: +Y, -Y, +X, -X, +Z, -Z.
constexpr std::array<Matrix3i, 6> UpRotations{{
    {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}},   // identity
    {{{1, 0, 0}, {0, -1, 0}, {0, 0, -1}}}, // half turn about X
    {{{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}}},  // -90 degrees about Z
    {{{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}},  // +90 degrees about Z
    {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}},  // +90 degrees about X
    {{{1, 0, 0}, {0, 0, 1}, {0, -1, 0}}},  // -90 degrees about X
}};

// k quarter turns about Y.
constexpr std::array<Matrix3i, 4> YawRotations{{
    {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}},
    {{{0, 0, 1}, {0, 1, 0}, {-1, 0, 0}}},
    {{{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}}},
    {{{0, 0, -1}, {0, 1, 0}, {1, 0, 0}}},
}};

constexpr std::array<Matrix3i, OrientationCount> MakeTable() {
    std::array<Matrix3i, OrientationCount> table{};
    for (std::size_t o = 0; o < OrientationCount; ++o) table[o] = Multiply(UpRotations[o / 4], YawRotations[o % 4]);
    return table;
}

constexpr std::array<Matrix3i, OrientationCount> Table = MakeTable();

Vec3i Turn(const Matrix3i& m, const Vec3i& p) {
    // About the cell's centre: p' = M (p - 8) + 8, in units.
    const Vec3i d{p[0] - Units / 2, p[1] - Units / 2, p[2] - Units / 2};
    Vec3i result{};
    for (int row = 0; row < 3; ++row)
        result[row] = m[row][0] * d[0] + m[row][1] * d[1] + m[row][2] * d[2] + Units / 2;
    return result;
}

} // namespace

const Matrix3i& OrientationMatrix(const std::size_t orientation) {
    GYO_ASSERT(orientation < OrientationCount);
    return Table[orientation];
}

Box RotateBox(const Box& box, const std::size_t orientation) {
    const auto& m = OrientationMatrix(orientation);
    const auto a = Turn(m, box.minimum);
    const auto b = Turn(m, box.maximum);
    Box result;
    for (int axis = 0; axis < 3; ++axis) {
        result.minimum[axis] = std::min(a[axis], b[axis]);
        result.maximum[axis] = std::max(a[axis], b[axis]);
    }
    return result;
}

} // namespace Gyo::Tools::VoxelMapEditor
