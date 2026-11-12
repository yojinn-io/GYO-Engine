#pragma once

#include "engine/render/PrimitiveMesh.hpp"
#include "engine/render/RenderTypes.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"
#include "RetroFPS/Sim/Arena.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace fps::pvp {
inline namespace sim {

// How the Client draws an arena's baked map (v8-product-arena-v2): one mesh of
// its faces, coloured by its palette texture. A schema 1 arena has no baked
// map and is drawn as its wall boxes.
[[nodiscard]] bool DrawsBakedMesh(const Engine::VoxelMesh::VoxelMeshGeometry& geometry) noexcept;
// The faces as Render's mesh: the same positions, uvs and indices.
[[nodiscard]] Engine::Render::MeshData ArenaRenderMesh(const Engine::VoxelMesh::VoxelMeshGeometry& geometry);
// The palette as an sRGB image over the geometry's own pixels (no copy).
[[nodiscard]] Engine::Render::ImageView ArenaPaletteImage(const Engine::VoxelMesh::VoxelMeshGeometry& geometry) noexcept;
// Which installed arena an id names (the Match chooses by id), so its baked
// map is drawn; nullopt for an id that is not installed.
[[nodiscard]] std::optional<std::size_t> InstalledArenaIndex(std::span<const Arena> installed,
                                                             std::string_view id) noexcept;

} // namespace sim
} // namespace fps::pvp
