#include "render/RenderQueue.hpp"
#include "engine/math/scalar/Constants.hpp"

#include <cmath>
#include <string>
#include <string_view>

namespace Engine::Render {
namespace {

[[nodiscard]] bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] RenderError Invalid(std::string_view message) {
    return RenderError::Make(RenderErrorCode::InvalidArgument, std::string(message));
}

[[nodiscard]] bool IsValid(const Transform3D& transform) noexcept {
    return IsFinite(transform.translation) && IsFinite(transform.rotationRadians) &&
           IsFinite(transform.scale);
}

[[nodiscard]] bool IsValid(const PerspectiveCamera3D& camera) noexcept {
    return IsFinite(camera.position) && IsFinite(camera.rotationRadians) &&
           IsFinite(camera.verticalFieldOfViewRadians) &&
           camera.verticalFieldOfViewRadians > 0.0F &&
           camera.verticalFieldOfViewRadians < Math::Pi &&
           IsFinite(camera.nearClip) && camera.nearClip > 0.0F &&
           IsFinite(camera.farClip) && camera.farClip > camera.nearClip;
}

} // namespace

RenderQueue::RenderQueue(const FrameDescription& frame)
    : frame_(frame) {}

void RenderQueue::Reset(const FrameDescription& frame) {
    frame_ = frame;
    camera_.reset();
    viewModelCamera_.reset();
    meshes_.clear();
    sprites_.clear();
}

void RenderQueue::SetCamera(const PerspectiveCamera3D& camera) {
    camera_ = camera;
}

void RenderQueue::ClearCamera() noexcept {
    camera_.reset();
}

void RenderQueue::SetViewModelCamera(const PerspectiveCamera3D& camera) {
    viewModelCamera_ = camera;
}

void RenderQueue::ClearViewModelCamera() noexcept {
    viewModelCamera_.reset();
}

Base::Result<void, RenderError> RenderQueue::Submit(
    const MeshSubmission& submission) {

    if (!submission.mesh) {
        return Base::Err(Invalid("RenderQueue: mesh submission requires a valid mesh handle"));
    }
    if (submission.layer != MeshLayer::World &&
        submission.layer != MeshLayer::ViewModel &&
        submission.layer != MeshLayer::WorldOverlay) {
        return Base::Err(Invalid("RenderQueue: mesh layer is invalid"));
    }
    const auto& camera = submission.layer == MeshLayer::ViewModel
        ? viewModelCamera_ : camera_;
    if (!camera || !IsValid(*camera)) {
        return Base::Err(Invalid(
            "RenderQueue: mesh submission requires a valid camera for its layer"));
    }
    if (!IsValid(submission.transform) || !IsFinite(submission.material.tint) ||
        !IsFinite(submission.uv.scale) || !IsFinite(submission.uv.offset)) {
        return Base::Err(Invalid("RenderQueue: mesh submission contains non-finite values"));
    }

    if (submission.material.shader.empty()) return Base::Err(Invalid("RenderQueue: shader ID is empty"));
    if (submission.surface != SurfaceMode::Opaque && submission.surface != SurfaceMode::AlphaMasked && submission.surface != SurfaceMode::Sky)
        return Base::Err(Invalid("RenderQueue: mesh surface mode is invalid"));
    if (submission.material.sampler != SamplerMode::LinearClamp && submission.material.sampler != SamplerMode::LinearWrap)
        return Base::Err(Invalid("RenderQueue: material sampler is invalid"));
    meshes_.push_back(submission);
    return {};
}

Base::Result<void, RenderError> RenderQueue::Submit(
    const SpriteSubmission& submission) {

    if (!IsFinite(submission.destinationPixels) || !IsFinite(submission.sourceUv) ||
        !IsFinite(submission.pivotNormalized) ||
        !IsFinite(submission.rotationRadians) || !IsFinite(submission.material.tint)) {
        return Base::Err(Invalid("RenderQueue: sprite submission contains non-finite values"));
    }
    if (submission.destinationPixels.width < 0.0F ||
        submission.destinationPixels.height < 0.0F ||
        submission.sourceUv.width < 0.0F || submission.sourceUv.height < 0.0F) {
        return Base::Err(Invalid("RenderQueue: sprite rectangles cannot have negative dimensions"));
    }
    if (submission.layer != CompositeLayer::Scene &&
        submission.layer != CompositeLayer::Overlay) {
        return Base::Err(Invalid("RenderQueue: sprite composite layer is invalid"));
    }

    if (submission.material.shader.empty()) return Base::Err(Invalid("RenderQueue: shader ID is empty"));
    if (submission.material.sampler != SamplerMode::LinearClamp && submission.material.sampler != SamplerMode::LinearWrap)
        return Base::Err(Invalid("RenderQueue: material sampler is invalid"));
    sprites_.push_back(submission);
    return {};
}

const FrameDescription& RenderQueue::Frame() const noexcept {
    return frame_;
}

const std::optional<PerspectiveCamera3D>& RenderQueue::Camera() const noexcept {
    return camera_;
}

const std::optional<PerspectiveCamera3D>& RenderQueue::ViewModelCamera() const noexcept {
    return viewModelCamera_;
}

std::span<const MeshSubmission> RenderQueue::Meshes() const noexcept {
    return meshes_;
}

std::span<const SpriteSubmission> RenderQueue::Sprites() const noexcept {
    return sprites_;
}

} // namespace Engine::Render
