#pragma once

#include "gyo/framework/FrameworkError.hpp"

#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/RenderDeviceTypes.hpp"
#include "engine/render/RenderQueue.hpp"
#include "engine/render/RenderTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace Engine::Render::Backend::SdlGpu {
class SdlGpuRenderDevice;
}

// The 3D preview a tool shows inside its window (docs/framework/preview.md):
// Engine's Renderer drawing on an offscreen GPU device beside the window's
// own renderer, every frame read back for upload. The framework puts the
// device, the renderer and the readback together; what is drawn, and how, is
// the client's: it submits Engine's MeshSubmissions to the queue the frame
// hands out and creates its meshes and textures on the device.
namespace Gyo::Tools::Framework {

struct PreviewOptions final {
    std::filesystem::path shaderBundle; // Engine's shader bundle (the directory of its manifest)
    std::uint32_t width{640}, height{480}; // the first frame's size; BeginFrame sets every frame's
    std::string driver{"auto"};         // SdlGpuOffscreenOptions::driver
};

class OffscreenPreview final {
public:
    // StartFailed when the bundle, the device or the renderer cannot start
    // (Engine's cause in detail). The platform must outlive the preview.
    [[nodiscard]] static Result<std::unique_ptr<OffscreenPreview>, PreviewError> Create(
        Engine::Platform::Sdl::SdlPlatform& platform, const PreviewOptions& options);
    ~OffscreenPreview();
    OffscreenPreview(const OffscreenPreview&) = delete;
    OffscreenPreview& operator=(const OffscreenPreview&) = delete;

    // The offscreen device, where the client creates meshes and textures; it
    // is Engine's IRenderDevice. The client releases what it created before
    // the preview is destroyed.
    [[nodiscard]] Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice& Device() noexcept;

    // Starts a frame of width x height pixels, seen from camera, cleared to
    // clearColor (linear), and hands out the empty queue the client submits
    // to; the queue lives until the next BeginFrame. DrawFailed when the size
    // is refused (zero). Programmer Error while a frame is open.
    [[nodiscard]] Result<Engine::Render::RenderQueue*, PreviewError> BeginFrame(
        std::uint32_t width, std::uint32_t height, const Engine::Render::PerspectiveCamera3D& camera,
        const Engine::Render::Color& clearColor);

    // Draws the open frame's queue and reads it back: Rgba8SRgb rows of
    // rowPitch bytes, this frame's and no earlier one's. DrawFailed when the
    // renderer or the readback fails. Programmer Error without an open frame.
    // After a successful BeginFrame the client calls it exactly once, also
    // when its own submission failed: what was submitted is then drawn and
    // becomes the device's last frame.
    [[nodiscard]] Result<Engine::Render::TextureReadback, PreviewError> EndFrame();

private:
    struct Impl;
    explicit OffscreenPreview(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace Gyo::Tools::Framework
