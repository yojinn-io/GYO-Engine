#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/base/Result.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/IRenderDevice.hpp"

namespace Engine::Render::Backend::SdlGpu {

struct SdlGpuOptions final {
    bool vsync{true};
    std::string driver{"auto"}; // auto, d3d12, vulkan, metal
    ShaderFormatMask availableShaderFormats{};
#if defined(NDEBUG)
    bool debugMode{false};
#else
    bool debugMode{true};
#endif
};

// A device that never claims the platform's window: each frame is drawn into a
// texture of its own (RGBA8 sRGB, the frame size) and read back on request.
// The window stays free for another renderer, such as the SDL 2D renderer of
// a tool's shell. The platform is still required: it owns the SDL subsystems
// the GPU drivers load through.
struct SdlGpuOffscreenOptions final {
    std::uint32_t width{}, height{}; // the frame size; both nonzero
    std::string driver{"auto"};      // auto, d3d12, vulkan, metal
    ShaderFormatMask availableShaderFormats{};
#if defined(NDEBUG)
    bool debugMode{false};
#else
    bool debugMode{true};
#endif
};

// SDL_GPU implementation. Native objects and synchronization remain private;
// shader artifacts, prepared passes and resource descriptors are GYO-owned.
class SdlGpuRenderDevice final : public IRenderDevice {
public:
    [[nodiscard]] static Base::Result<std::unique_ptr<SdlGpuRenderDevice>, RenderError>
    Create(
        Platform::Sdl::SdlPlatform& platform,
        const SdlGpuOptions& options = {});

    // An offscreen device (see SdlGpuOffscreenOptions). AcquireFrame always
    // returns a frame of the current frame size, in Rgba8SRgb, without waiting
    // for earlier frames; SubmitFrame draws into the device's own texture and
    // returns Presented. A caller that does not ReadFrame every frame paces
    // itself: nothing else holds the CPU back from the GPU.
    [[nodiscard]] static Base::Result<std::unique_ptr<SdlGpuRenderDevice>, RenderError>
    CreateOffscreen(
        Platform::Sdl::SdlPlatform& platform,
        const SdlGpuOffscreenOptions& options);

    ~SdlGpuRenderDevice() override;

    SdlGpuRenderDevice(const SdlGpuRenderDevice&) = delete;
    SdlGpuRenderDevice& operator=(const SdlGpuRenderDevice&) = delete;
    SdlGpuRenderDevice(SdlGpuRenderDevice&&) = delete;
    SdlGpuRenderDevice& operator=(SdlGpuRenderDevice&&) = delete;

    [[nodiscard]] Base::Result<MeshHandle, RenderError> CreateMesh(
        const MeshView& mesh) override;
    [[nodiscard]] Base::Result<TextureHandle, RenderError> CreateTexture(
        const ImageView& image) override;
    [[nodiscard]] Base::Result<void, RenderError> UpdateMeshVertices(
        MeshHandle handle, std::span<const Vertex3D> vertices) override;
    [[nodiscard]] Base::Result<void, RenderError> ReleaseMesh(
        MeshHandle handle) override;
    [[nodiscard]] Base::Result<void, RenderError> ReleaseTexture(
        TextureHandle handle) override;
    [[nodiscard]] RenderDeviceInfo GetInfo() const override;
    [[nodiscard]] Base::Result<ShaderHandle, RenderError> CreateShader(const ShaderArtifact&) override;
    [[nodiscard]] Base::Result<void, RenderError> ReleaseShader(ShaderHandle) override;
    [[nodiscard]] Base::Result<TextureHandle, RenderError> CreateTexture(const TextureDesc&) override;
    [[nodiscard]] Base::Result<PipelineHandle, RenderError> CreatePipeline(const PipelineDesc&) override;
    [[nodiscard]] Base::Result<void, RenderError> ReleasePipeline(PipelineHandle) override;
    [[nodiscard]] Base::Result<std::optional<AcquiredFrame>, RenderError> AcquireFrame() override;
    [[nodiscard]] Base::Result<PresentStatus, RenderError> SubmitFrame(const AcquiredFrame&, const PreparedFrame&) override;
    void AbandonFrame(const AcquiredFrame&) noexcept override;
    [[nodiscard]] Base::Result<TextureReadback, RenderError> ReadTexture(TextureHandle) override;

    // Offscreen devices only (UnsupportedOperation otherwise). Neither may run
    // between AcquireFrame and SubmitFrame or AbandonFrame (InvalidArgument).
    // The next acquired frame has the new size; a size of zero is refused.
    [[nodiscard]] Base::Result<void, RenderError> SetFrameSize(std::uint32_t width, std::uint32_t height);
    // The last submitted frame, waiting for the GPU to finish it (as ReadTexture
    // does): Rgba8SRgb rows of rowPitch bytes. InvalidArgument when no frame of
    // the current frame size was submitted (none yet, or the size changed since),
    // or when the last frame was abandoned or stored nothing into the frame
    // target (no pass with an invalid color handle and storeColor).
    [[nodiscard]] Base::Result<TextureReadback, RenderError> ReadFrame();

private:
    struct Impl;

    explicit SdlGpuRenderDevice(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Render::Backend::SdlGpu
