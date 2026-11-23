#include "engine/camera/Camera.hpp"

#include <algorithm>
#include <cmath>

#include "engine/math/linear/Vec4.hpp"

namespace GYO::Engine::Camera {

namespace {

using Code = ViewportConversionFailureCode;

ViewportConversionFailure Fail(const Code code) { return ViewportConversionFailure::Make(code, {}); }

// Bevy's Viewport::clamp_to_size, per axis, after a negative position is taken as 0.
void ClampAxis(std::int32_t& position, std::int32_t& size, const std::int32_t target) {
    position = std::max(position, 0);
    size = std::max(size, 0);
    if (size + position > target) {
        if (position < target) {
            size = target - position;
        } else if (target > 0) {
            position = target - 1;
            size = 1;
        } else {
            position = 0;
            size = 0;
        }
    }
}

Math::Matrix4 ProjectionMatrix(const Projection& projection, const float logicalWidth, const float logicalHeight) {
    if (const auto* perspective = std::get_if<PerspectiveProjection>(&projection)) {
        return Math::MakePerspective(perspective->verticalFov, logicalWidth / logicalHeight, perspective->nearClip,
                                     perspective->farClip);
    }
    const auto& orthographic = std::get<OrthographicProjection>(projection);
    // Bevy's OrthographicProjection::update.
    float width = logicalWidth;
    float height = logicalHeight;
    if (const auto* fixed = std::get_if<ScalingFixed>(&orthographic.scalingMode)) {
        width = fixed->width;
        height = fixed->height;
    }
    const float originX = width * orthographic.viewportOrigin.x;
    const float originY = height * orthographic.viewportOrigin.y;
    return Math::MakeOrthographic(orthographic.scale * -originX, orthographic.scale * (width - originX),
                                  orthographic.scale * -originY, orthographic.scale * (height - originY),
                                  orthographic.nearClip, orthographic.farClip);
}

struct LogicalRect {
    Math::Vec2 minimum;
    Math::Vec2 size;
};

LogicalRect LogicalViewport(const ComputedCamera& camera) {
    const float s = camera.scaleFactor;
    return {{static_cast<float>(camera.viewport.position.x) / s, static_cast<float>(camera.viewport.position.y) / s},
            {static_cast<float>(camera.viewport.size.x) / s, static_cast<float>(camera.viewport.size.y) / s}};
}

// Bevy's viewport_to_ndc: the position relative to the viewport, y flipped (NDC has +Y up).
Math::Vec2 ToNdc(const LogicalRect& rect, const Math::Vec2 position) {
    const float x = (position.x - rect.minimum.x) / rect.size.x * 2.0F - 1.0F;
    const float y = (position.y - rect.minimum.y) / rect.size.y * 2.0F - 1.0F;
    return {x, -y};
}

// The homogeneous product divided by w (Bevy's project_point3a).
Math::Vec3 Project(const Math::Matrix4& matrix, const Math::Vec3 point) {
    const Math::Vec4 clip = Math::Multiply(matrix, Math::MakePoint4(point));
    return {clip.x / clip.w, clip.y / clip.w, clip.z / clip.w};
}

bool Finite(const Math::Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// The view-space point of an NDC position: near plane at depth 0, far plane at 1 (math.md; Bevy's reversed depth
// takes the near plane at 1).
Base::Result<Math::Vec3, ViewportConversionFailure> NdcToView(const ComputedCamera& camera, const Math::Vec2 ndc,
                                                             const float depth) {
    const std::optional<Math::Matrix4> viewFromClip = Math::Inverse(camera.clipFromView);
    if (!viewFromClip) return Base::Err(Fail(Code::InvalidData));
    const Math::Vec3 point = Project(*viewFromClip, {ndc.x, ndc.y, depth});
    if (!Finite(point)) return Base::Err(Fail(Code::InvalidData));
    return point;
}

} // namespace

void CameraTargetSizes::Set(const CameraTarget target, const TargetSize size) {
    auto at = std::lower_bound(sizes_.begin(), sizes_.end(), target.id,
                               [](const auto& entry, std::uint32_t id) { return entry.first < id; });
    if (at != sizes_.end() && at->first == target.id) {
        at->second = size;
    } else {
        sizes_.insert(at, {target.id, size});
    }
}

void CameraTargetSizes::Remove(const CameraTarget target) {
    auto at = std::lower_bound(sizes_.begin(), sizes_.end(), target.id,
                               [](const auto& entry, std::uint32_t id) { return entry.first < id; });
    if (at != sizes_.end() && at->first == target.id) sizes_.erase(at);
}

const TargetSize* CameraTargetSizes::Find(const CameraTarget target) const {
    auto at = std::lower_bound(sizes_.begin(), sizes_.end(), target.id,
                               [](const auto& entry, std::uint32_t id) { return entry.first < id; });
    return at != sizes_.end() && at->first == target.id ? &at->second : nullptr;
}

ComputedCamera Compute(const Camera& camera, const Projection& projection, const TargetSize* size) {
    ComputedCamera out;
    if (size == nullptr) return out;
    out.targetPhysicalSize = size->physicalSize;
    out.scaleFactor = size->scaleFactor;
    Viewport viewport = camera.viewport.value_or(Viewport{{0, 0}, size->physicalSize, 0.0F, 1.0F});
    ClampAxis(viewport.position.x, viewport.size.x, size->physicalSize.x);
    ClampAxis(viewport.position.y, viewport.size.y, size->physicalSize.y);
    out.viewport = viewport;
    if (viewport.size.x <= 0 || viewport.size.y <= 0 || !(size->scaleFactor > 0.0F)) return out;
    const float logicalWidth = static_cast<float>(viewport.size.x) / size->scaleFactor;
    const float logicalHeight = static_cast<float>(viewport.size.y) / size->scaleFactor;
    out.clipFromView = ProjectionMatrix(projection, logicalWidth, logicalHeight);
    out.valid = true;
    return out;
}

Base::Result<Math::Vec2, ViewportConversionFailure> WorldToViewport(const ComputedCamera& camera,
                                                                   const Transform::GlobalTransform& view,
                                                                   const Math::Vec3 world) {
    if (!camera.valid) return Base::Err(Fail(Code::NoViewportSize));
    const std::optional<Math::Matrix4> viewFromWorld = Math::Inverse(view.matrix);
    if (!viewFromWorld) return Base::Err(Fail(Code::InvalidData));
    const Math::Vec3 ndc = Project(camera.clipFromView, Math::TransformPoint(*viewFromWorld, world));
    if (std::isnan(ndc.x) || std::isnan(ndc.y) || std::isnan(ndc.z)) {
        return Base::Err(Fail(Code::InvalidData));
    }
    if (ndc.z < 0.0F) return Base::Err(Fail(Code::PastNearPlane));
    if (ndc.z > 1.0F) return Base::Err(Fail(Code::PastFarPlane));
    const LogicalRect rect = LogicalViewport(camera);
    return Math::Vec2{(ndc.x + 1.0F) / 2.0F * rect.size.x + rect.minimum.x,
                      (-ndc.y + 1.0F) / 2.0F * rect.size.y + rect.minimum.y};
}

Base::Result<Math::Ray, ViewportConversionFailure> ViewportToWorld(const ComputedCamera& camera,
                                                                  const Transform::GlobalTransform& view,
                                                                  const Math::Vec2 position) {
    if (!camera.valid) return Base::Err(Fail(Code::NoViewportSize));
    const Math::Vec2 ndc = ToNdc(LogicalViewport(camera), position);
    auto nearPoint = NdcToView(camera, ndc, 0.0F);
    if (!nearPoint) return Base::Err(std::move(nearPoint).error());
    auto farPoint = NdcToView(camera, ndc, 1.0F);
    if (!farPoint) return Base::Err(std::move(farPoint).error());
    const Math::Vec3 origin = Math::TransformPoint(view.matrix, *nearPoint);
    const Math::Vec3 direction = Math::TransformVector(view.matrix, *farPoint - *nearPoint);
    if (!Finite(origin) || !Finite(direction) || (direction.x == 0.0F && direction.y == 0.0F && direction.z == 0.0F)) {
        return Base::Err(Fail(Code::InvalidData));
    }
    return Math::Ray{origin, direction};
}

Base::Result<Math::Vec2, ViewportConversionFailure> ViewportToWorld2d(const ComputedCamera& camera,
                                                                     const Transform::GlobalTransform& view,
                                                                     const Math::Vec2 position) {
    if (!camera.valid) return Base::Err(Fail(Code::NoViewportSize));
    auto nearPoint = NdcToView(camera, ToNdc(LogicalViewport(camera), position), 0.0F);
    if (!nearPoint) return Base::Err(std::move(nearPoint).error());
    const Math::Vec3 world = Math::TransformPoint(view.matrix, *nearPoint);
    if (!Finite(world)) return Base::Err(Fail(Code::InvalidData));
    return Math::Vec2{world.x, world.y};
}

} // namespace GYO::Engine::Camera
