#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "engine/base/Result.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/IRenderDevice.hpp"

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

// SDL_GPU implementation. Native objects and synchronization remain private;
// shader artifacts, prepared passes and resource descriptors are GYO-owned.
class SdlGpuRenderDevice final : public IRenderDevice {
public:
    [[nodiscard]] static Base::Result<std::unique_ptr<SdlGpuRenderDevice>, RenderError>
    Create(
        Platform::Sdl::SdlPlatform& platform,
        const SdlGpuOptions& options = {});

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

private:
    struct Impl;

    explicit SdlGpuRenderDevice(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Render::Backend::SdlGpu
