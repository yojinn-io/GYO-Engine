#include "render/RenderQueue.hpp"

#include <cmath>
#include <string>
#include <string_view>

namespace Engine::Render {
namespace {

[[nodiscard]] bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool IsFinite(Float2 value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y);
}

[[nodiscard]] bool IsFinite(Float3 value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

[[nodiscard]] bool IsFinite(Color value) noexcept {
    return IsFinite(value.red) && IsFinite(value.green) &&
           IsFinite(value.blue) && IsFinite(value.alpha);
}

[[nodiscard]] bool IsFinite(Rect value) noexcept {
    return IsFinite(value.x) && IsFinite(value.y) &&
           IsFinite(value.width) && IsFinite(value.height);
}

[[nodiscard]] RenderError Invalid(std::string_view message) {
    return RenderError::Make(RenderErrorCode::InvalidArgument, std::string(message));
}

[[nodiscard]] bool IsValid(const Transform3D& transform) noexcept {
    return IsFinite(transform.translation) && IsFinite(transform.rotationRadians) &&
           IsFinite(transform.scale);
}

} // namespace

RenderQueue::RenderQueue(const FrameDescription& frame)
    : frame_(frame) {}

void RenderQueue::Reset(const FrameDescription& frame) {
    frame_ = frame;
    camera_.reset();
    meshes_.clear();
    sprites_.clear();
}

void RenderQueue::SetCamera(const PerspectiveCamera3D& camera) {
    camera_ = camera;
}

void RenderQueue::ClearCamera() noexcept {
    camera_.reset();
}

Base::Result<void, RenderError> RenderQueue::Submit(
    const MeshSubmission& submission) {
    using Result = Base::Result<void, RenderError>;

    if (!submission.mesh) {
        return Result::Err(Invalid("RenderQueue: mesh submission requires a valid mesh handle"));
    }
    if (!IsValid(submission.transform) || !IsFinite(submission.tint) ||
        !IsFinite(submission.uv.scale) || !IsFinite(submission.uv.offset)) {
        return Result::Err(Invalid("RenderQueue: mesh submission contains non-finite values"));
    }

    meshes_.push_back(submission);
    return Result::Ok();
}

Base::Result<void, RenderError> RenderQueue::Submit(
    const SpriteSubmission& submission) {
    using Result = Base::Result<void, RenderError>;

    if (!IsFinite(submission.destinationPixels) || !IsFinite(submission.sourceUv) ||
        !IsFinite(submission.pivotNormalized) ||
        !IsFinite(submission.rotationRadians) || !IsFinite(submission.tint)) {
        return Result::Err(Invalid("RenderQueue: sprite submission contains non-finite values"));
    }
    if (submission.destinationPixels.width < 0.0F ||
        submission.destinationPixels.height < 0.0F ||
        submission.sourceUv.width < 0.0F || submission.sourceUv.height < 0.0F) {
        return Result::Err(Invalid("RenderQueue: sprite rectangles cannot have negative dimensions"));
    }
    if (submission.layer != CompositeLayer::Scene &&
        submission.layer != CompositeLayer::Overlay) {
        return Result::Err(Invalid("RenderQueue: sprite composite layer is invalid"));
    }

    sprites_.push_back(submission);
    return Result::Ok();
}

const FrameDescription& RenderQueue::Frame() const noexcept {
    return frame_;
}

const std::optional<PerspectiveCamera3D>& RenderQueue::Camera() const noexcept {
    return camera_;
}

std::span<const MeshSubmission> RenderQueue::Meshes() const noexcept {
    return meshes_;
}

std::span<const SpriteSubmission> RenderQueue::Sprites() const noexcept {
    return sprites_;
}

} // namespace Engine::Render
