#pragma once

#include "engine/render/PrimitiveMesh.hpp"
#include "engine/render/RenderTypes.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

namespace fps::pvp {

// How the Client draws an arena's baked map (v8-product-arena-v2): one mesh of
// its faces, coloured by its palette texture. A schema 1 arena has no baked
// map and is drawn as its wall boxes.
[[nodiscard]] bool DrawsBakedMesh(const Engine::VoxelMesh::VoxelMeshGeometry& geometry) noexcept;
// The faces as Render's mesh: the same positions, uvs and indices.
[[nodiscard]] Engine::Render::MeshData ArenaRenderMesh(const Engine::VoxelMesh::VoxelMeshGeometry& geometry);
// The palette as an sRGB image over the geometry's own pixels (no copy).
[[nodiscard]] Engine::Render::ImageView ArenaPaletteImage(const Engine::VoxelMesh::VoxelMeshGeometry& geometry) noexcept;

} // namespace fps::pvp
