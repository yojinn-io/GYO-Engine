#pragma once

#include "gyo/voxel_map_editor/Editing.hpp"
#include "gyo/voxel_map_editor/VoxelMap.hpp"
#include "gyo/voxel_map_editor/Visibility.hpp"

#include "gyo/framework/Preview.hpp"

#include "engine/render/RenderTypes.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>

// The map on the framework's offscreen preview (docs/framework/preview.md):
// the editor creates and holds the preview, puts Engine's expansion of each
// baked chunk on its device (Design Invariant 7) and draws them with Engine's
// unlit shader and the palette texture, as the game does. Only the chunks an
// edit changed are uploaded again.
namespace Gyo::Tools::VoxelMapEditor {

class PreviewScene final {
public:
    [[nodiscard]] static Result<std::unique_ptr<PreviewScene>> Create(Engine::Platform::Sdl::SdlPlatform& platform,
                                                                      const std::filesystem::path& shaderBundle,
                                                                      const std::string& driver);
    ~PreviewScene();
    PreviewScene(const PreviewScene&) = delete;
    PreviewScene& operator=(const PreviewScene&) = delete;

    // Puts the changed chunks (and the palette image when it changed) on the
    // device; a chunk without geometry is no longer drawn. On an error the
    // chunks put so far stay and the rest keep their old meshes.
    Result<void> Apply(const MapPreview::Changes& changes);
    // Drops every chunk and the palette (before showing another map).
    void Clear();
    [[nodiscard]] std::size_t ChunkCount() const noexcept { return meshes_.size(); }
    // One frame of the map; the readback is this frame's (Rgba8SRgb). Only the
    // chunks within drawDistance (world units, x and z) of center that overlap
    // the camera's frustum are drawn (Visibility.hpp).
    Result<Engine::Render::TextureReadback> Render(const Engine::Render::PerspectiveCamera3D& camera,
                                                   Engine::Math::Vec3 center, float drawDistance,
                                                   std::uint32_t width, std::uint32_t height);
    // The chunks the last Render drew.
    [[nodiscard]] std::size_t DrawnCount() const noexcept { return drawn_; }

private:
    PreviewScene() = default;

    std::unique_ptr<Framework::OffscreenPreview> preview_;
    struct ChunkMesh final {
        Engine::Render::MeshHandle mesh{};
        Engine::Math::Aabb bounds{}; // of the mesh's vertices, world space
    };
    std::map<Vec3i, ChunkMesh> meshes_;
    std::size_t drawn_{};
    Engine::Render::TextureHandle palette_{};
};

} // namespace Gyo::Tools::VoxelMapEditor
