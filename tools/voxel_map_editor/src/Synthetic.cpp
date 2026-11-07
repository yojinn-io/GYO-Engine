#include "gyo/voxel_map_editor/Synthetic.hpp"

#include <random>

// Synthetic maps of the measurements (v8-arch-big-map-tool-chunked "量測",
// v8-tool-voxel-editor-measure). Their rules are fixed by those declarations.
namespace Gyo::Tools::VoxelMapEditor {

// The building-like synthetic map: minstd_rand(20261106), 256 x 48 x 256 cells
// (16 x 3 x 16 chunks). A one-cell floor every 8 cells, a wall every 16 cells
// along x and z with random door holes, and per room random stairs and slabs.
GridDocument BuildingGrid() {
    GridDocument grid;
    grid.shapes = DefaultShapes(); // cube, slab, stair
    grid.blocks = {{"floor", 0, {200, 180, 150, 255}},
                   {"wall", 0, {120, 130, 140, 255}},
                   {"stairs", 2, {60, 120, 200, 255}},
                   {"slab", 1, {220, 40, 40, 255}}};
    std::minstd_rand random(20261106);
    const auto pick = [&random](const int bound) { return static_cast<std::int32_t>(random() % static_cast<unsigned>(bound)); };
    constexpr std::int32_t Size = 256, Height = 48, Storey = 8, Room = 16;
    for (std::int32_t floor = 0; floor < Height; floor += Storey) {
        for (std::int32_t z = 0; z < Size; ++z)
            for (std::int32_t x = 0; x < Size; ++x) grid.Set({x, floor, z}, {1, 0});
        // Walls on x = 16k (running along z) and z = 16k (running along x), between this floor and the next.
        for (std::int32_t line = 0; line < Size; line += Room) {
            for (std::int32_t segment = 0; segment < Size; segment += Room) {
                const bool door = pick(10) < 6;
                const auto doorAt = segment + 2 + pick(Room - 5);
                for (std::int32_t along = segment; along < segment + Room; ++along)
                    for (std::int32_t y = floor + 1; y < floor + Storey; ++y) {
                        if (door && along >= doorAt && along < doorAt + 2 && y < floor + 4) continue;
                        grid.Set({line, y, along}, {2, 0});
                    }
                const bool otherDoor = pick(10) < 6;
                const auto otherAt = segment + 2 + pick(Room - 5);
                for (std::int32_t along = segment; along < segment + Room; ++along)
                    for (std::int32_t y = floor + 1; y < floor + Storey; ++y) {
                        if (otherDoor && along >= otherAt && along < otherAt + 2 && y < floor + 4) continue;
                        grid.Set({along, y, line}, {2, 0});
                    }
            }
        }
        // Per room: a stair run of up to 6 steps along +x (30 %), and up to 4 slabs.
        for (std::int32_t rz = 0; rz < Size; rz += Room)
            for (std::int32_t rx = 0; rx < Size; rx += Room) {
                if (pick(10) < 3) {
                    const auto z = rz + 3 + pick(Room - 6);
                    for (std::int32_t step = 0; step < 6; ++step)
                        grid.Set({rx + 3 + step, floor + 1 + step, z}, {3, static_cast<std::uint8_t>(pick(4))});
                }
                const auto slabs = pick(5);
                for (std::int32_t i = 0; i < slabs; ++i)
                    grid.Set({rx + 2 + pick(Room - 3), floor + 1, rz + 2 + pick(Room - 3)}, {4, 0});
            }
    }
    return grid;
}

GridDocument Km2GroundGrid() {
    GridDocument grid;
    grid.shapes = DefaultShapes();
    grid.blocks = {{"ground", 0, {120, 150, 90, 255}}, {"wall", 0, {120, 130, 140, 255}}};
    constexpr std::int32_t Size = Km2Cells;
    for (std::int32_t z = 0; z < Size; ++z)
        for (std::int32_t x = 0; x < Size; ++x) {
            grid.Set({x, 0, z}, {1, 0});
            // A wall one cell wide and two high every 64 cells along x and z.
            if (x % 64 == 0 || z % 64 == 0) {
                grid.Set({x, 1, z}, {2, 0});
                grid.Set({x, 2, z}, {2, 0});
            }
        }
    return grid;
}

GridDocument Km2BuildingGrid() {
    // The building map tiled over 2000 x 2000 cells: every 256 x 256 tile is the same building (one seed), so the
    // tiles line up with chunks and the chunks are copied whole; chunks past 2000 cells are left out.
    const GridDocument tile = BuildingGrid();
    GridDocument grid;
    grid.shapes = tile.shapes;
    grid.blocks = tile.blocks;
    constexpr std::int32_t TileChunks = 256 / ChunkSize;
    constexpr std::int32_t MapChunks = Km2Cells / ChunkSize; // 125
    for (std::int32_t tz = 0; tz * TileChunks < MapChunks; ++tz)
        for (std::int32_t tx = 0; tx * TileChunks < MapChunks; ++tx)
            for (const auto& [at, cells] : tile.chunks) {
                const Vec3i key{at[0] + tx * TileChunks, at[1], at[2] + tz * TileChunks};
                if (key[0] >= MapChunks || key[2] >= MapChunks) continue;
                grid.chunks.emplace(key, cells);
            }
    return grid;
}

std::optional<GridDocument> SyntheticByName(const std::string_view name) {
    if (name == "building") return BuildingGrid();
    if (name == "km2-ground") return Km2GroundGrid();
    if (name == "km2-building") return Km2BuildingGrid();
    return std::nullopt;
}

} // namespace Gyo::Tools::VoxelMapEditor
