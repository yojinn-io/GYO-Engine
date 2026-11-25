#include "voxel_map_editor/Synthetic.hpp"

#include <random>

// Synthetic maps of the measurements, as master made them (v8-arch-big-map-tool-chunked, v8-tool-voxel-editor-measure),
// so a bake here and master's can be compared; the km2 maps take their size.
namespace voxel_map_editor {

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

GridDocument GroundGrid(const std::int32_t size) {
    GridDocument grid;
    grid.shapes = DefaultShapes();
    grid.blocks = {{"ground", 0, {120, 150, 90, 255}}, {"wall", 0, {120, 130, 140, 255}}};
    for (std::int32_t z = 0; z < size; ++z)
        for (std::int32_t x = 0; x < size; ++x) {
            grid.Set({x, 0, z}, {1, 0});
            // A wall one cell wide and two high every 64 cells along x and z.
            if (x % 64 == 0 || z % 64 == 0) {
                grid.Set({x, 1, z}, {2, 0});
                grid.Set({x, 2, z}, {2, 0});
            }
        }
    return grid;
}

GridDocument TiledBuildingGrid(const std::int32_t size) {
    // The building map tiled over size x size cells: every 256 x 256 tile is the same building (one seed), so the
    // tiles line up with chunks and the chunks are copied whole; chunks past size cells are left out.
    const GridDocument tile = BuildingGrid();
    GridDocument grid;
    grid.shapes = tile.shapes;
    grid.blocks = tile.blocks;
    constexpr std::int32_t kTileChunks = 256 / kChunkSize;
    const std::int32_t mapChunks = size / kChunkSize; // 125 for a km
    for (std::int32_t tz = 0; tz * kTileChunks < mapChunks; ++tz)
        for (std::int32_t tx = 0; tx * kTileChunks < mapChunks; ++tx)
            for (const auto& [at, cells] : tile.chunks) {
                const Vec3i key{at[0] + tx * kTileChunks, at[1], at[2] + tz * kTileChunks};
                if (key[0] >= mapChunks || key[2] >= mapChunks) continue;
                grid.chunks.emplace(key, cells);
            }
    return grid;
}

std::optional<GridDocument> SyntheticByName(const std::string_view name, const std::int32_t size) {
    if (size < kChunkSize || size > kMaxSyntheticCells || size % kChunkSize != 0) return std::nullopt;
    if (name == "building") return BuildingGrid();
    if (name == "km2-ground") return GroundGrid(size);
    if (name == "km2-building") return TiledBuildingGrid(size);
    return std::nullopt;
}

} // namespace voxel_map_editor
