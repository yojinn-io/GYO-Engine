#include "window/PreviewScene.hpp"

#include "engine/base/Error.hpp"
#include "engine/render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"

#include <utility>
#include <vector>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

namespace Render = Engine::Render;

// The preview's background, linear.
constexpr Render::Color kBackground{0.16F, 0.17F, 0.19F, 1.0F};

Engine::Base::Err<VoxelMapError> PreviewFailed(std::string message, std::string detail) {
    return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::PreviewFailed, std::move(message), std::move(detail)));
}

} // namespace

Result<std::unique_ptr<PreviewScene>> PreviewScene::Create(Engine::Platform::Sdl::SdlPlatform& platform,
                                                           const std::filesystem::path& shaderBundle,
                                                           const std::string& driver) {
    Framework::PreviewOptions options;
    options.shaderBundle = shaderBundle;
    options.driver = driver;
    auto preview = Framework::OffscreenPreview::Create(platform, options);
    if (!preview) return PreviewFailed("the preview cannot start", Engine::Base::CauseDetail(preview.error()));
    std::unique_ptr<PreviewScene> scene(new PreviewScene());
    scene->preview_ = std::move(preview.value());
    return scene;
}

PreviewScene::~PreviewScene() { Release(); }

void PreviewScene::Release() {
    // The map's device objects go before the preview's device.
    if (!preview_) return;
    if (mesh_.IsValid()) static_cast<void>(preview_->Device().ReleaseMesh(mesh_));
    if (palette_.IsValid()) static_cast<void>(preview_->Device().ReleaseTexture(palette_));
    mesh_ = {};
    palette_ = {};
}

Result<void> PreviewScene::Show(const Engine::VoxelMesh::VoxelMeshGeometry& geometry) {
    Release();
    if (geometry.vertices.empty() || geometry.indices.empty() || geometry.paletteRgba8.empty()) return {};
    // Engine's expansion as Render's mesh: the same positions, uvs and indices.
    std::vector<Render::Vertex3D> vertices;
    vertices.reserve(geometry.vertices.size());
    for (const auto& vertex : geometry.vertices) vertices.push_back({vertex.position, vertex.uv});
    auto& device = preview_->Device();
    auto mesh = device.CreateMesh(Render::MeshView{vertices, geometry.indices});
    if (!mesh) return PreviewFailed("the map mesh cannot be created", Engine::Base::CauseDetail(mesh.error()));
    auto palette = device.CreateTexture(Render::ImageView{geometry.paletteWidth, geometry.paletteHeight, 0,
                                                          geometry.paletteRgba8, Render::TextureColorSpace::SRgb});
    if (!palette) {
        static_cast<void>(device.ReleaseMesh(mesh.value()));
        return PreviewFailed("the palette texture cannot be created", Engine::Base::CauseDetail(palette.error()));
    }
    mesh_ = mesh.value();
    palette_ = palette.value();
    return {};
}

Result<Render::TextureReadback> PreviewScene::Render(const Render::PerspectiveCamera3D& camera,
                                                     const std::uint32_t width, const std::uint32_t height) {
    auto queue = preview_->BeginFrame(width, height, camera, kBackground);
    if (!queue) return PreviewFailed("the preview cannot start the frame", Engine::Base::CauseDetail(queue.error()));
    // The frame is open from here: EndFrame runs on every path.
    std::string submitError;
    if (mesh_.IsValid()) {
        // As the game draws the baked map: one draw, Engine's unlit shader, the palette texture.
        Render::MeshSubmission draw;
        draw.mesh = mesh_;
        draw.material.texture = palette_;
        draw.material.tint = {1, 1, 1, 1};
        if (auto submitted = queue.value()->Submit(draw); !submitted) submitError = Engine::Base::CauseDetail(submitted.error());
    }
    auto frame = preview_->EndFrame();
    if (!submitError.empty()) return PreviewFailed("the map cannot be drawn", submitError);
    if (!frame) return PreviewFailed("the preview cannot draw", Engine::Base::CauseDetail(frame.error()));
    return std::move(frame.value());
}

} // namespace Gyo::Tools::VoxelMapEditor
