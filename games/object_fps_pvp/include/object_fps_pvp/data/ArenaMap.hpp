#pragma once

#include "engine/math/geometry/Aabb.hpp"
#include "engine/model/ModelData.hpp"
#include "engine/voxel_map/VoxelMap.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace object_fps_pvp::data {

// An arena's map: the chunked voxel map a tool baked (gyo.voxel_map with its gyo.voxel_region files; Engine's voxel_map
// Module reads and validates it, docs/architecture/target/v2/voxel-map.zh-Hant.md). The game reads the whole map
// (big-map declaration section 15).
struct ArenaMap final {
    GYO::Engine::VoxelMap::VoxelMapDocument index;
    // One per index entry, in index order.
    std::vector<GYO::Engine::VoxelMap::VoxelRegionDocument> regions;
};

// Reads a map from its index text and its regions' texts: readRegion(file) gives the text of a region file named in
// the index (relative to the index's directory). nullopt with error when a text breaks the contract (the Module's
// message and path) or a region cannot be read (readRegion's error).
using RegionReader = std::function<std::optional<std::string>(const std::string& file, std::string& error)>;
[[nodiscard]] std::optional<ArenaMap> ReadArenaMap(std::string_view indexText, const RegionReader& readRegion, std::string& error);

// One part of a map read elsewhere (an asset store's index and regions, in index order), copied for a job that outlives
// the reading: Walls keeps the collision boxes only, Faces the quads only (a Match never holds faces, a picture never
// holds boxes). Chunks left with nothing are dropped.
enum class MapPart : std::uint8_t { Walls, Faces };
[[nodiscard]] ArenaMap CopyMapPart(const GYO::Engine::VoxelMap::VoxelMapDocument& index,
                                   std::span<const GYO::Engine::VoxelMap::VoxelRegionDocument* const> regions, MapPart part);

// The collision boxes of the whole map in world coordinates: every chunk's, in index order, then chunk order.
[[nodiscard]] std::vector<GYO::Engine::Math::Aabb> MapColliders(const ArenaMap& map);

// One region's picture: its faces as one model (one node at the origin, one mesh; uv into the map's palette image) and
// its world box (for culling). A region without a face has no picture.
struct RegionPicture final {
    std::shared_ptr<const GYO::Engine::Model::ModelData> model;
    GYO::Engine::Math::Aabb bounds{};
};
[[nodiscard]] std::vector<RegionPicture> RegionPictures(const ArenaMap& map);
// The picture of one region (an index of map.regions): RegionPictures' item for it, or nothing when it has no face.
// Each region on its own, so the regions of a map can be made on several threads (each result the same).
[[nodiscard]] std::optional<RegionPicture> RegionPictureOf(const ArenaMap& map, std::size_t region);

} // namespace object_fps_pvp::data
