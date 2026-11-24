#pragma once

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace object_fps_pvp::data {

// The voxel map editor's baked map (gyo.voxel_mesh version 1): the arena's walls and the faces that draw them.
// The format is the tool's Data Contract (docs/architecture/voxel-mesh.md, "The single-file format"); the game
// reads the members it needs here and nothing in Engine reads it (port declaration, PV8). Every geometric value is
// an integer in 1/16 of a cell from origin, so every platform expands it to the same floats.

// One face: its minimum corner (x, y, z), the direction it looks at (0 to 5: +X, -X, +Y, -Y, +Z, -Z), its two
// in-plane sizes (X faces run along Z and Y, Y faces along X and Z, Z faces along X and Y) and its palette entry.
struct VoxelQuad final {
    std::array<std::int32_t, 3> corner{};
    std::uint8_t direction = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint32_t palette = 0;
};

// One collision box in units, min < max on every axis.
struct VoxelBox final {
    std::array<std::int32_t, 3> minimum{};
    std::array<std::int32_t, 3> maximum{};
};

// sRGB RGBA8, one colour per face direction (the quad's direction indexes it).
using VoxelColor = std::array<std::uint8_t, 4>;
using VoxelPaletteEntry = std::array<VoxelColor, 6>;

struct VoxelMeshFile final {
    std::array<std::int32_t, 3> origin{};
    float cellSize = 1.0F;
    std::vector<VoxelPaletteEntry> palette;
    std::vector<VoxelQuad> quads;
    std::vector<VoxelBox> colliders;
};

// Reads a gyo.voxel_mesh version 1 text. nullopt with error naming what was wrong (a schema or version this
// reader does not know, a missing member, a value of the wrong kind, a box with min >= max, a direction outside
// 0 to 5 or a palette index outside the palette).
[[nodiscard]] std::optional<VoxelMeshFile> ParseVoxelMeshFile(std::string_view text, std::string& error);

// A position in units (from origin) in world coordinates: float(units + 16 x origin) x cellSize / 16, the
// contract's formula (the integer sum is exact, the conversion and the product round once, / 16 is exact).
[[nodiscard]] GYO::Engine::Math::Vec3 UnitsToWorld(const VoxelMeshFile& file, const std::array<std::int32_t, 3>& units) noexcept;

// The collision boxes in world coordinates, in the file's order.
[[nodiscard]] std::vector<GYO::Engine::Math::Aabb> ColliderBoxes(const VoxelMeshFile& file);

} // namespace object_fps_pvp::data

namespace object_fps_pvp::data {

// The faces as one triangle list (four vertices and six indices per quad, in quad order), each vertex with its
// face's normal and the uv of the centre of its colour's texel in PaletteImage.
struct VoxelSurface final {
    std::vector<GYO::Engine::Math::Vec3> positions;
    std::vector<GYO::Engine::Math::Vec3> normals;
    std::vector<std::array<float, 2>> uvs;
    std::vector<std::uint32_t> indices;
};
[[nodiscard]] VoxelSurface ExpandQuads(const VoxelMeshFile& file);

// The palette as RGBA8 pixels, top row first: one column per entry, one row per face direction (+X, -X, +Y, -Y,
// +Z, -Z).
struct VoxelPaletteImage final {
    std::uint32_t width = 0;
    std::uint32_t height = 6;
    std::vector<std::uint8_t> pixels;
};
[[nodiscard]] VoxelPaletteImage PaletteImage(const VoxelMeshFile& file);

} // namespace object_fps_pvp::data
