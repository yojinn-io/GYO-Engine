#pragma once

#include "gyo/model_viewer/ModelViewerError.hpp"

#include "gyo/framework/Preview.hpp"
#include "gyo/framework/PreviewCamera.hpp"

#include "engine/model/Animation.hpp"
#include "engine/model/ModelAsset.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/RenderDeviceTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

// The 3D preview (Design Invariant 5, the (B') drawing of
// docs/tools/plans/v8/model-viewer.md): Engine's ModelRenderer draws the posed
// model into a frame of the framework's offscreen preview (the offscreen GPU
// device, Engine's Renderer and the readback, docs/framework/preview.md), which
// leaves the window to the shell's SDL renderer. The viewer has no projection,
// sorting or shading of its own; the camera is the framework's OrbitCamera.
namespace Gyo::Tools::ModelViewer {

using Framework::OrbitCamera;

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
    // PreviewFailed when the framework's preview cannot start (its cause in detail).
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
    void SetSkeleton(bool shown) noexcept;

    // Draws the shown model in pose (a pose of that model) into a frame of
    // width x height pixels and reads the frame back: the bytes to upload,
    // Rgba8SRgb rows of rowPitch bytes.
    [[nodiscard]] Result<Engine::Render::TextureReadback, ModelViewerError> Render(
        const Engine::Model::Pose& pose, const OrbitCamera& camera, std::uint32_t width, std::uint32_t height);

    // The framework's preview, for tests that read the frame themselves.
    [[nodiscard]] Framework::OffscreenPreview& Preview() noexcept;

private:
    struct Impl;
    explicit PreviewRenderer(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace Gyo::Tools::ModelViewer
