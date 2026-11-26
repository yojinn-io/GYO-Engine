#include "object_fps_pvp/data/ArenaMap.hpp"

#include <algorithm>
#include <utility>

namespace object_fps_pvp::data {

namespace VM = GYO::Engine::VoxelMap;
namespace Math = GYO::Engine::Math;
namespace Model = GYO::Engine::Model;

namespace {

std::string Describe(const VM::VoxelMapError& error) {
    return error.detail.empty() ? error.message : error.message + " at " + error.detail;
}

Math::Aabb Union(const Math::Aabb& a, const Math::Aabb& b) {
    return {{std::min(a.minimum.x, b.minimum.x), std::min(a.minimum.y, b.minimum.y), std::min(a.minimum.z, b.minimum.z)},
            {std::max(a.maximum.x, b.maximum.x), std::max(a.maximum.y, b.maximum.y), std::max(a.maximum.z, b.maximum.z)}};
}

} // namespace

std::optional<ArenaMap> ReadArenaMap(const std::string_view indexText, const RegionReader& readRegion, std::string& error) {
    error.clear();
    auto index = VM::ParseVoxelMap(indexText);
    if (!index) {
        error = "the map's index: " + Describe(index.error());
        return std::nullopt;
    }
    ArenaMap map;
    map.index = std::move(*index);
    map.regions.reserve(map.index.regions.size());
    for (const VM::VoxelMapRegion& entry : map.index.regions) {
        auto text = readRegion(entry.file, error);
        if (!text) return std::nullopt;
        auto region = VM::ParseVoxelRegion(*text);
        if (!region) {
            error = entry.file + ": " + Describe(region.error());
            return std::nullopt;
        }
        map.regions.push_back(std::move(*region));
    }
    std::vector<const VM::VoxelRegionDocument*> regions;
    for (const VM::VoxelRegionDocument& region : map.regions) regions.push_back(&region);
    if (auto checked = VM::CheckVoxelMap(map.index, regions); !checked) {
        error = "the map: " + Describe(checked.error());
        return std::nullopt;
    }
    return map;
}

ArenaMap CopyMapPart(const VM::VoxelMapDocument& index, const std::span<const VM::VoxelRegionDocument* const> regions,
                     const MapPart part) {
    ArenaMap map;
    map.index = index;
    map.regions.reserve(regions.size());
    for (const VM::VoxelRegionDocument* region : regions) {
        VM::VoxelRegionDocument copy;
        copy.at = region->at;
        copy.textSha256 = region->textSha256;
        for (const VM::VoxelChunkDocument& chunk : region->chunks) {
            VM::VoxelChunkDocument kept{chunk.at, {}, {}};
            if (part == MapPart::Walls) kept.colliders = chunk.colliders;
            else kept.quads = chunk.quads;
            if (!kept.colliders.empty() || !kept.quads.empty()) copy.chunks.push_back(std::move(kept));
        }
        map.regions.push_back(std::move(copy));
    }
    return map;
}

std::vector<Math::Aabb> MapColliders(const ArenaMap& map) {
    std::vector<Math::Aabb> boxes;
    for (const VM::VoxelRegionDocument& region : map.regions) {
        for (const VM::VoxelChunkDocument& chunk : region.chunks) {
            if (chunk.colliders.empty()) continue;
            VM::VoxelChunkDocument boxesOnly{chunk.at, {}, chunk.colliders};
            const VM::VoxelGeometry expanded = VM::ExpandVoxelChunk(boxesOnly, map.index);
            boxes.insert(boxes.end(), expanded.colliders.begin(), expanded.colliders.end());
        }
    }
    return boxes;
}

std::vector<RegionPicture> RegionPictures(const ArenaMap& map) {
    std::vector<RegionPicture> pictures;
    for (const VM::VoxelRegionDocument& region : map.regions) {
        VM::VoxelGeometry geometry;
        std::optional<Math::Aabb> bounds;
        for (const VM::VoxelChunkDocument& chunk : region.chunks) {
            if (chunk.quads.empty()) continue;
            VM::VoxelChunkDocument facesOnly{chunk.at, chunk.quads, {}};
            VM::AppendVoxelChunk(geometry, facesOnly, map.index);
            const Math::Aabb box = VM::ChunkWorldBounds(chunk.at, map.index);
            bounds = bounds ? Union(*bounds, box) : box;
        }
        if (geometry.positions.empty()) continue;
        Model::ModelData model;
        model.nodes.push_back(Model::Node{});
        Model::Mesh mesh;
        mesh.positions = std::move(geometry.positions);
        mesh.normals = std::move(geometry.normals);
        mesh.uvs = std::move(geometry.uvs);
        mesh.indices = std::move(geometry.indices);
        model.meshes.push_back(std::move(mesh));
        pictures.push_back({std::make_shared<const Model::ModelData>(std::move(model)), *bounds});
    }
    return pictures;
}

} // namespace object_fps_pvp::data
