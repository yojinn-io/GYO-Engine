#pragma once

#include "voxel_map_editor/Grid.hpp"

#include "engine/voxel_map/VoxelMap.hpp"

#include <cstddef>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The chunked bake: a grid baked into the voxel_map Module's gyo.voxel_map index and gyo.voxel_region files, one chunk
// at a time. The grid's chunks (16^3 cells from origin) are the map's chunks, and the map's origin is the grid's origin.
// The tool writes, the Module validates: every text is read back with ParseVoxelMap and ParseVoxelRegion.
namespace voxel_map_editor {

inline constexpr std::string_view kMapBuildVersion = "voxel_map_editor 2";
inline constexpr std::string_view kMapIndexFile = "map.json";
inline constexpr std::string_view kMapRegionDirectory = "regions";

// A baked map in memory. index comes from the Module's ParseVoxelMap, so chunks can be checked against it; until
// WriteMap its source_sha256 and its regions' sha256 and file are placeholders (its regions' keys are the real ones).
struct BakedMap final {
    GYO::Engine::VoxelMap::VoxelMapDocument index;
    // Every chunk with a filled cell, by chunk coordinate; each was accepted by the Module's ValidateVoxelChunk
    // against index.
    std::map<Vec3i, GYO::Engine::VoxelMap::VoxelChunkDocument> chunks;
};

// Bakes every chunk: faces culled per box side against the neighbouring cell's whole side (cullface, reading
// neighbours across chunk borders), merged inside the chunk per direction, plane and palette entry by a fixed greedy
// scan; collision boxes merged per chunk (MergeBoxes). The palette is every block of the grid in grid order. EmptyMap
// for a grid without filled cells; BuildRefused when the Module does not accept the index or a chunk.
[[nodiscard]] Result<BakedMap> BakeMap(const GridDocument& grid);

struct MapRegionText final {
    std::string file; // relative to the map directory, as the index lists it
    std::string text;
};

struct MapText final {
    std::string index;
    std::vector<MapRegionText> regions; // in key order
};

// The texts of a baked map. A region's file is regions/r_<x>_<z>_<h>.json, h the first 16 hexadecimal digits of the
// region text's SHA-256. Every text is read back with ParseVoxelMap and ParseVoxelRegion(text, index, at) and compared
// with the baked map (BuildRefused when the Module refuses it or it reads back different). sourceBytes are the grid
// file's bytes, for source_sha256.
[[nodiscard]] Result<MapText> WriteMap(const BakedMap& map, std::span<const std::byte> sourceBytes);

// Saving, in this order, so that a failure leaves the previous map readable: region files first (each by atomic
// replace; a file already holding the same bytes is left alone), then the index, then removing the region files the
// previous index listed and the new one does not (only names of the region file form under regions/; a file already
// gone is fine). engine/io cannot list a directory, so a file no index lists (left by a save that stopped before its
// index) stays. SaveMap runs the steps; they are separate for tests. ListedRegionFiles is empty when there is no
// index or the Module refuses it.
[[nodiscard]] std::vector<std::string> ListedRegionFiles(const std::string& directory);
[[nodiscard]] Result<void> WriteRegionFiles(const std::string& directory, const MapText& text);
[[nodiscard]] Result<void> ReplaceMapIndex(const std::string& directory, const MapText& text);
[[nodiscard]] Result<void> RemoveStaleRegionFiles(const std::string& directory, const MapText& text,
                                                  std::span<const std::string> previous);
[[nodiscard]] Result<void> SaveMap(const std::string& directory, const MapText& text);

// Whether a file name (without directory) has the form WriteMap gives region files.
[[nodiscard]] bool IsRegionFileName(std::string_view name) noexcept;

// What a map holds, read from its directory with the Module only (as a game reads it). Errors: ReadFailed, or
// BuildRefused when the Module refuses the index or a region.
struct MapSummary final {
    GYO::Engine::VoxelMap::VoxelMapDocument index;
    std::size_t chunks{};
    std::size_t quads{};
    std::size_t colliders{};
    std::size_t bytes{}; // the index and every region file
};
[[nodiscard]] Result<MapSummary> ReadMapSummary(const std::string& directory);

} // namespace voxel_map_editor
