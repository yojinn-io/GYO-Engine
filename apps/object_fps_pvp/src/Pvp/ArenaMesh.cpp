#include "RetroFPS/Pvp/ArenaMesh.hpp"

namespace fps::pvp {

bool DrawsBakedMesh(const Engine::VoxelMesh::VoxelMeshGeometry& geometry) noexcept {
    return !geometry.vertices.empty() && !geometry.indices.empty() && !geometry.paletteRgba8.empty();
}

Engine::Render::MeshData ArenaRenderMesh(const Engine::VoxelMesh::VoxelMeshGeometry& geometry) {
    Engine::Render::MeshData mesh;
    mesh.vertices.reserve(geometry.vertices.size());
    for (const auto& vertex : geometry.vertices) mesh.vertices.push_back({vertex.position, vertex.uv});
    mesh.indices = geometry.indices;
    return mesh;
}

Engine::Render::ImageView ArenaPaletteImage(const Engine::VoxelMesh::VoxelMeshGeometry& geometry) noexcept {
    return {geometry.paletteWidth, geometry.paletteHeight, 0, geometry.paletteRgba8,
            Engine::Render::TextureColorSpace::SRgb};
}

std::optional<std::size_t> InstalledArenaIndex(const std::span<const Arena> installed,
                                               const std::string_view id) noexcept {
    for (std::size_t i = 0; i < installed.size(); ++i)
        if (installed[i].id == id) return i;
    return std::nullopt;
}

} // namespace fps::pvp
