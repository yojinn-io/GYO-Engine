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

PreviewScene::~PreviewScene() { Clear(); }

void PreviewScene::Clear() {
    // The map's device objects go before the preview's device.
    if (!preview_) return;
    for (const auto& [at, mesh] : meshes_) static_cast<void>(preview_->Device().ReleaseMesh(mesh));
    if (palette_.IsValid()) static_cast<void>(preview_->Device().ReleaseTexture(palette_));
    meshes_.clear();
    palette_ = {};
}

Result<void> PreviewScene::Apply(const MapPreview::Changes& changes) {
    auto& device = preview_->Device();
    if (changes.palette) {
        const auto& image = *changes.palette;
        auto palette = device.CreateTexture(
            Render::ImageView{image.width, image.height, 0, image.rgba8, Render::TextureColorSpace::SRgb});
        if (!palette) return PreviewFailed("the palette texture cannot be created", Engine::Base::CauseDetail(palette.error()));
        if (palette_.IsValid()) static_cast<void>(device.ReleaseTexture(palette_));
        palette_ = palette.value();
    }
    for (const auto& [at, geometry] : changes.chunks) {
        if (const auto old = meshes_.find(at); old != meshes_.end()) {
            static_cast<void>(device.ReleaseMesh(old->second));
            meshes_.erase(old);
        }
        if (!geometry || geometry->vertices.empty() || geometry->indices.empty()) continue;
        // Engine's expansion as Render's mesh: the same positions, uvs and indices.
        std::vector<Render::Vertex3D> vertices;
        vertices.reserve(geometry->vertices.size());
        for (const auto& vertex : geometry->vertices) vertices.push_back({vertex.position, vertex.uv});
        auto mesh = device.CreateMesh(Render::MeshView{vertices, geometry->indices});
        if (!mesh) return PreviewFailed("a chunk mesh cannot be created", Engine::Base::CauseDetail(mesh.error()));
        meshes_.emplace(at, mesh.value());
    }
    return {};
}

Result<Render::TextureReadback> PreviewScene::Render(const Render::PerspectiveCamera3D& camera,
                                                     const std::uint32_t width, const std::uint32_t height) {
    auto queue = preview_->BeginFrame(width, height, camera, kBackground);
    if (!queue) return PreviewFailed("the preview cannot start the frame", Engine::Base::CauseDetail(queue.error()));
    // The frame is open from here: EndFrame runs on every path.
    std::string submitError;
    if (palette_.IsValid()) {
        // As the game draws the baked map: one draw per chunk, Engine's unlit shader, the palette texture.
        for (const auto& [at, mesh] : meshes_) {
            Render::MeshSubmission draw;
            draw.mesh = mesh;
            draw.material.texture = palette_;
            draw.material.tint = {1, 1, 1, 1};
            if (auto submitted = queue.value()->Submit(draw); !submitted) {
                submitError = Engine::Base::CauseDetail(submitted.error());
                break;
            }
        }
    }
    auto frame = preview_->EndFrame();
    if (!submitError.empty()) return PreviewFailed("the map cannot be drawn", submitError);
    if (!frame) return PreviewFailed("the preview cannot draw", Engine::Base::CauseDetail(frame.error()));
    return std::move(frame.value());
}

} // namespace Gyo::Tools::VoxelMapEditor
