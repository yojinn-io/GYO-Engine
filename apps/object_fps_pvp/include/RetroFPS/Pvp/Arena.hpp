#pragma once

#include "engine/collision/Collision.hpp"
#include "engine/collision/StaticAabbGrid.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fps::pvp {

struct SpawnPoint final {
    Engine::Math::Vec3 position{};
    float yaw{};
};

// Product-owned versioned content shared by runtime collision and rendering.
struct Arena final {
    std::uint32_t version{1};
    std::string id;
    float width{};
    float depth{};
    float cellSize{1.0F};
    float movementSpeed{3.0F};
    float bodyHeight{1.8F};
    float radius{0.25F};
    float eyeHeight{1.6F};
    std::vector<Engine::Math::Aabb> walls;
    std::vector<SpawnPoint> spawns;
    float jumpHeight{0.6F};
    float gravity{18.0F};
    // Derived, not content: the broadphase over walls (v8-product-pa-broadphase).
    // Load builds it; code that changes walls calls BuildWallGrid again. walls
    // stay the authority (digest, wire, rendering read walls).
    std::shared_ptr<const Engine::Collision::StaticAabbGrid> wallGrid;

    [[nodiscard]] bool Validate(std::string& error) const;
    // Builds wallGrid from walls (cell length cellSize). walls and cellSize must
    // already be valid (Validate checks them first).
    void BuildWallGrid();
    // The grid every wall query uses. It asserts only that the grid has as many
    // boxes as walls: changing a wall in place without BuildWallGrid goes
    // unnoticed (only Load writes walls in the product). An arena without walls
    // needs no build.
    [[nodiscard]] const Engine::Collision::StaticAabbGrid& WallGrid() const;
    // Arena files: schema 1 lists walls; schema 2 names a gyo.voxel_mesh next
    // to it ("geometry", a plain file name), whose collision boxes become the
    // walls in the file's order. The file schema is not Arena::version, which
    // stays 1: the members and the digest do not change (v8-tool-voxel-arena-v2).
    // geometry, when given, receives the baked mesh (empty for schema 1).
    [[nodiscard]] static std::optional<Arena> Load(
        const std::filesystem::path& path, std::string& error,
        Engine::VoxelMesh::VoxelMeshGeometry* geometry = nullptr);
};

// Content digest of a loaded arena (pv6 contract §2): 64-bit FNV-1a over the
// parsed members in declaration order, big-endian, floats as their binary32
// bits. Line endings, whitespace, key order and number spelling do not matter.
// Zero is reserved for "missing": a caller treats it as a load failure.
[[nodiscard]] std::uint64_t ArenaContentDigest(const Arena& arena);

// The most colliders an arena may have (v8-product-pa-broadphase): within the
// StaticAabbGrid limits (2^22 cells, fewer than 2^32 boxes and registrations).
inline constexpr std::size_t MaximumArenaColliders = std::size_t{1} << 20;

// Arena v1 spawn counts (pv6 contract §2, 2026-10-07 revision).
inline constexpr std::size_t MinimumArenaSpawns = 2;
inline constexpr std::size_t MaximumArenaSpawns = 64;

// The Match hosts only an arena with at least MaxPlayers spawns. Binaries from
// before the revision reject any arena whose spawn count is not two, so mixing
// them with a hostable arena always fails at the arena digest.
[[nodiscard]] bool ArenaHostsRoom(const Arena& arena, std::string& error);

// Client content: the arenas a Client installs (arenas.json, version 1), plain
// file names next to the list, in order. The Match chooses the arena; a
// Client selects the installed one with the Match's id (ClientConnection), and
// shows the first before it joins. Ids are unique and every arena shares the
// first one's body height, which the character presentation is built for.
// geometries, when given, receives each arena's baked mesh in the same order.
[[nodiscard]] std::optional<std::vector<Arena>> LoadInstalledArenas(
    const std::filesystem::path& list, std::string& error,
    std::vector<Engine::VoxelMesh::VoxelMeshGeometry>* geometries = nullptr);

} // namespace fps::pvp
