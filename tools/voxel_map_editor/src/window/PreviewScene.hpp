#pragma once

#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "gyo/framework/Preview.hpp"

#include "engine/render/RenderTypes.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <filesystem>
#include <memory>
#include <string>

// The map on the framework's offscreen preview (docs/framework/preview.md):
// the editor creates and holds the preview, puts Engine's expansion of the
// baked map on its device (Design Invariant 7) and draws it with Engine's
// unlit shader and the palette texture, as the game does.
namespace Gyo::Tools::VoxelMapEditor {

class PreviewScene final {
public:
    [[nodiscard]] static Result<std::unique_ptr<PreviewScene>> Create(Engine::Platform::Sdl::SdlPlatform& platform,
                                                                      const std::filesystem::path& shaderBundle,
                                                                      const std::string& driver);
    ~PreviewScene();
    PreviewScene(const PreviewScene&) = delete;
    PreviewScene& operator=(const PreviewScene&) = delete;

    // Replaces the drawn map; empty geometry draws nothing.
    Result<void> Show(const Engine::VoxelMesh::VoxelMeshGeometry& geometry);
    // One frame of the map; the readback is this frame's (Rgba8SRgb).
    Result<Engine::Render::TextureReadback> Render(const Engine::Render::PerspectiveCamera3D& camera,
                                                   std::uint32_t width, std::uint32_t height);

private:
    PreviewScene() = default;
    void Release();

    std::unique_ptr<Framework::OffscreenPreview> preview_;
    Engine::Render::MeshHandle mesh_{};
    Engine::Render::TextureHandle palette_{};
};

} // namespace Gyo::Tools::VoxelMapEditor
