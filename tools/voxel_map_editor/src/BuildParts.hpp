#pragma once

#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

// Pieces of the build shared by the single file (Build.cpp) and the chunked
// map (MapBuild.cpp), so both cull and colour faces by the same code.
namespace Gyo::Tools::VoxelMapEditor::BuildParts {

using Quad = std::array<std::int32_t, 7>; // x, y, z, dir, w, h, palette (Engine's order)

// Whether a shape's boxes cover a whole side of the cell (Engine's face
// order: +X, -X, +Y, -Y, +Z, -Z).
inline bool CoversSide(const std::vector<Box>& boxes, const int side) {
    const int axis = side / 2;
    const bool positive = side % 2 == 0;
    const int u = axis == 0 ? 2 : 0; // the two in-plane axes
    const int v = axis == 1 ? 2 : 1;
    std::array<bool, Units * Units> covered{};
    for (const auto& box : boxes) {
        if ((positive ? box.maximum[axis] : box.minimum[axis]) != (positive ? Units : 0)) continue;
        for (int a = box.minimum[u]; a < box.maximum[u]; ++a)
            for (int b = box.minimum[v]; b < box.maximum[v]; ++b) covered[static_cast<std::size_t>(a * Units + b)] = true;
    }
    return std::all_of(covered.begin(), covered.end(), [](const bool c) { return c; });
}

// The face of a box looking along dir, in Engine's quad form.
inline Quad FaceQuad(const Box& b, const int dir, const std::int32_t palette) {
    const int axis = dir / 2;
    const bool positive = dir % 2 == 0;
    Quad q{};
    q[0] = b.minimum[0];
    q[1] = b.minimum[1];
    q[2] = b.minimum[2];
    q[static_cast<std::size_t>(axis)] = positive ? b.maximum[axis] : b.minimum[axis];
    q[3] = dir;
    switch (axis) {
    case 0: q[4] = b.maximum[2] - b.minimum[2]; q[5] = b.maximum[1] - b.minimum[1]; break; // (Z, Y)
    case 1: q[4] = b.maximum[0] - b.minimum[0]; q[5] = b.maximum[2] - b.minimum[2]; break; // (X, Z)
    default: q[4] = b.maximum[0] - b.minimum[0]; q[5] = b.maximum[1] - b.minimum[1]; break; // (X, Y)
    }
    q[6] = palette;
    return q;
}

inline std::uint8_t Shade(const std::uint8_t channel, const std::uint32_t percent) {
    return static_cast<std::uint8_t>((channel * percent + 50u) / 100u);
}

inline std::string Ints(const std::initializer_list<std::int64_t> values) {
    std::string out = "[";
    bool first = true;
    for (const auto v : values) {
        if (!first) out += ", ";
        first = false;
        out += std::to_string(v);
    }
    return out + "]";
}

} // namespace Gyo::Tools::VoxelMapEditor::BuildParts
