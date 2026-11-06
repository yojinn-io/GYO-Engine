#pragma once

#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The chunked bake (v8-arch-big-map-tool-chunked): a grid baked into Engine's
// gyo.voxel_map index and gyo.voxel_region files, one chunk at a time, so an
// edit rebakes only the chunks it touched. The grid's chunks (16^3 cells from
// origin) are Engine's chunks, and the map's origin is the grid's origin.
namespace Gyo::Tools::VoxelMapEditor {

inline constexpr std::string_view MapBuildVersion = "voxel_map_editor 2";
inline constexpr std::string_view MapIndexFile = "map.json";
inline constexpr std::string_view MapRegionDirectory = "regions";

// A baked map in memory. index comes from Engine's ParseVoxelMap, so chunks
// can be checked against it; until WriteMap its source_sha256 and its regions'
// sha256 and file are placeholders (its regions' keys are the real ones).
struct BakedMap final {
    Engine::VoxelMesh::VoxelMapDocument index;
    // Every chunk with a filled cell, by chunk coordinate; each was accepted
    // by Engine's ValidateVoxelChunk against index.
    std::map<Vec3i, Engine::VoxelMesh::VoxelChunkDocument> chunks;
    // The shape library and each block's shape the chunks were baked with: a
    // change of either changes every chunk using it, so RebakeChunks then
    // bakes everything.
    std::vector<Shape> shapes;
    std::vector<std::uint32_t> blockShapes;
};

// Bakes every chunk: faces culled as the single file does (cullface, reading
// neighbours across chunk borders), merged inside the chunk per direction,
// plane and palette entry by a fixed greedy scan; collision boxes merged per
// chunk (MergeBoxes). The palette is every block of the grid in grid order.
// EmptyMap for a grid without filled cells; BuildRefused when Engine does not
// accept the index or a chunk.
[[nodiscard]] Result<BakedMap> BakeMap(const GridDocument& grid);

// Bakes the listed chunks again (a chunk left empty is dropped) and updates
// the index when the bounds or the regions changed. When the grid's palette,
// shape library, blocks' shapes, origin or cell size differ from the map's, it
// bakes everything (BakeMap): only cell edits are rebaked chunk by chunk.
// On an error the map is left as it was.
[[nodiscard]] Result<void> RebakeChunks(const GridDocument& grid, BakedMap& map, std::span<const Vec3i> chunks);

// The chunks a change of these cells (relative to origin) can change: each
// cell's own chunk and, for a cell on a chunk's side, the chunk across that
// side (whose faces it may hide or uncover).
[[nodiscard]] std::set<Vec3i> ChunksTouchedBy(std::span<const Vec3i> cells);

struct MapRegionText final {
    std::string file; // relative to the map directory, as the index lists it
    std::string text;
};

struct MapText final {
    std::string index;
    std::vector<MapRegionText> regions; // in key order
};

// The texts of a baked map. A region's file is regions/r_<x>_<z>_<h>.json,
// h the first 16 hexadecimal digits of the region text's SHA-256. Every text
// is read back with ParseVoxelMap and ParseVoxelRegion and compared with the
// baked map (BuildRefused when Engine refuses it or it reads back different).
// sourceBytes are the grid file's bytes, for source_sha256.
[[nodiscard]] Result<MapText> WriteMap(const BakedMap& map, std::span<const std::byte> sourceBytes);

// Saving, in this order, so that a failure leaves the previous map readable:
// region files first (each by atomic replace; a file already holding the same
// bytes is left alone), then the index, then removing the region files the
// previous index listed and the new one does not (only names of the region
// file form under regions/; a file already gone is fine). engine/io cannot
// list a directory, so a file no index lists (left by a save that stopped
// before its index) stays. SaveMap runs the steps; they are separate for tests.
// ListedRegionFiles is empty when there is no index or Engine refuses it.
[[nodiscard]] std::vector<std::string> ListedRegionFiles(const std::string& directory);
[[nodiscard]] Result<void> WriteRegionFiles(const std::string& directory, const MapText& text);
[[nodiscard]] Result<void> ReplaceMapIndex(const std::string& directory, const MapText& text);
[[nodiscard]] Result<void> RemoveStaleRegionFiles(const std::string& directory, const MapText& text,
                                                  std::span<const std::string> previous);
[[nodiscard]] Result<void> SaveMap(const std::string& directory, const MapText& text);

// Whether a file name (without directory) has the form WriteMap gives region files.
[[nodiscard]] bool IsRegionFileName(std::string_view name) noexcept;

} // namespace Gyo::Tools::VoxelMapEditor
