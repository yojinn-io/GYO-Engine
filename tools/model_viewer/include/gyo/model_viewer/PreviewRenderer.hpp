#pragma once

#include "gyo/model_viewer/ModelViewerError.hpp"

#include "engine/math/linear/Vec3.hpp"
#include "engine/model/Animation.hpp"
#include "engine/model/ModelAsset.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/RenderDeviceTypes.hpp"
#include "engine/render/RenderTypes.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace Engine::Render::Backend::SdlGpu {
class SdlGpuRenderDevice;
}

// The 3D preview (Design Invariant 5, the (B') drawing of
// docs/tools/plans/v8/model-viewer.md): Engine's ModelRenderer and Renderer
// draw the posed model on an offscreen GPU device, which leaves the
// window to the shell's SDL renderer, and the frame is read back for upload.
// The viewer has no projection, sorting or shading of its own.
namespace Gyo::Tools::ModelViewer {

// A camera turning about a point: yaw about the vertical, then pitch (positive
// looks down), at distance from target. Engine's camera convention: the
// camera's rotation is Z, then Y, then X Euler angles, +Z forward.
struct OrbitCamera final {
    Engine::Math::Vec3 target{};
    float yawRadians{};
    float pitchRadians{0.3F};
    float distance{3.0F};
    float verticalFieldOfViewRadians{0.9F};

    [[nodiscard]] Engine::Render::PerspectiveCamera3D Camera() const noexcept;
};

// The camera that fits pose: the target at the middle of the model's points
// (mesh vertices placed by their nodes, and nodes), the distance from their
// extent, keeping yaw and pitch.
[[nodiscard]] OrbitCamera FrameModel(const Engine::Model::ModelAsset& model, const Engine::Model::Pose& pose,
                                     OrbitCamera camera) noexcept;

struct PreviewOptions final {
    std::filesystem::path shaderBundle; // Engine's builtin bundle (bundle.json's directory)
    std::uint32_t width{640}, height{480};
    std::string driver{"auto"};
    bool skeleton{true}; // the bones as thin boxes over the mesh (no depth test)
};

class PreviewRenderer final {
public:
    // PreviewFailed when the device, the shaders or
    // the renderer cannot start (Engine's cause in detail).
    [[nodiscard]] static Result<std::unique_ptr<PreviewRenderer>, ModelViewerError> Create(
        Engine::Platform::Sdl::SdlPlatform& platform, const PreviewOptions& options);
    ~PreviewRenderer();
    PreviewRenderer(const PreviewRenderer&) = delete;
    PreviewRenderer& operator=(const PreviewRenderer&) = delete;

    // The model to draw, with every material's base colour as its tint (no
    // textures: which texture a material gets is the product's); nullptr shows
    // none. A model without meshes draws only its skeleton.
    [[nodiscard]] Result<void, ModelViewerError> Show(std::shared_ptr<const Engine::Model::ModelAsset> model,
                                                      const Engine::Model::Pose& pose);
    [[nodiscard]] Result<void, ModelViewerError> Resize(std::uint32_t width, std::uint32_t height);
    void SetSkeleton(bool shown) noexcept;

    // Draws the shown model in pose (a pose of that model) and reads the frame
    // back: the bytes to upload, Rgba8SRgb rows of rowPitch bytes.
    [[nodiscard]] Result<const Engine::Render::TextureReadback*, ModelViewerError> Render(
        const Engine::Model::Pose& pose, const OrbitCamera& camera);

    // The offscreen device, for tests that read the frame themselves.
    [[nodiscard]] Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice& Device() noexcept;

private:
    struct Impl;
    explicit PreviewRenderer(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

// The texture of the window's renderer that shows the preview, and its size.
struct PreviewTexture final {
    Engine::Render::Backend::Sdl::SdlTextureHandle handle;
    std::uint32_t width{}, height{};
};

// Puts frame into texture: a texture of another size (or none) is released
// and a new one of the frame's size made, then the frame's bytes are uploaded
// as they were read back. Call it before the texture is drawn in a frame,
// never after: ImGui draws a frame's images after Draw (ImGuiTextureId).
[[nodiscard]] Result<void, ModelViewerError> UploadFrame(Engine::Render::Backend::Sdl::SdlRenderer& renderer,
                                                         PreviewTexture& texture,
                                                         const Engine::Render::TextureReadback& frame);

} // namespace Gyo::Tools::ModelViewer
