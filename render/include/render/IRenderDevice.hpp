#pragma once

#include "engine/base/Result.hpp"
#include "render/RenderError.hpp"
#include "render/RenderDeviceTypes.hpp"
#include "render/RenderTypes.hpp"

namespace Engine::Render {

class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    [[nodiscard]] virtual Base::Result<MeshHandle, RenderError> CreateMesh(
        const MeshView& mesh) = 0;
    [[nodiscard]] virtual Base::Result<TextureHandle, RenderError> CreateTexture(
        const ImageView& image) = 0;

    // Replace all vertices of an existing mesh without changing its vertex
    // count, indices, or handle. The call consumes the span before returning;
    // successful updates are visible to subsequent SubmitFrame calls. Backends
    // must preserve data used by frames already submitted to the GPU.
    [[nodiscard]] virtual Base::Result<void, RenderError> UpdateMeshVertices(
        MeshHandle, std::span<const Vertex3D>) {
        return Base::Result<void, RenderError>::Err(RenderError::Make(
            RenderErrorCode::UnsupportedOperation,
            "render device does not support mesh vertex updates"));
    }

    [[nodiscard]] virtual Base::Result<void, RenderError> ReleaseMesh(
        MeshHandle handle) = 0;
    [[nodiscard]] virtual Base::Result<void, RenderError> ReleaseTexture(
        TextureHandle handle) = 0;

    [[nodiscard]] virtual RenderDeviceInfo GetInfo() const = 0;
    [[nodiscard]] virtual Base::Result<ShaderHandle, RenderError> CreateShader(const ShaderArtifact&) = 0;
    [[nodiscard]] virtual Base::Result<void, RenderError> ReleaseShader(ShaderHandle) = 0;
    [[nodiscard]] virtual Base::Result<TextureHandle, RenderError> CreateTexture(
        const TextureDesc& description) = 0;
    [[nodiscard]] virtual Base::Result<PipelineHandle, RenderError> CreatePipeline(
        const PipelineDesc& description) = 0;
    [[nodiscard]] virtual Base::Result<void, RenderError> ReleasePipeline(PipelineHandle) = 0;
    // A minimized surface returns nullopt and leaves no outstanding frame.
    [[nodiscard]] virtual Base::Result<std::optional<AcquiredFrame>, RenderError> AcquireFrame() = 0;
    // A valid token on the creation thread is consumed even on validation or
    // submission failure. An invalid token never affects the active frame.
    // All CPU bytes are consumed before returning; backend synchronization
    // protects in-flight resources.
    [[nodiscard]] virtual Base::Result<PresentStatus, RenderError> SubmitFrame(
        const AcquiredFrame&, const PreparedFrame&) = 0;
    // May submit an empty native command buffer when an acquired swapchain
    // forbids cancellation. It never executes unsubmitted prepared draws.
    virtual void AbandonFrame(const AcquiredFrame&) noexcept = 0;
    [[nodiscard]] virtual Base::Result<TextureReadback, RenderError> ReadTexture(
        TextureHandle) = 0;
};

} // namespace Engine::Render
