#pragma once

// Cameras (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 4.2; Design Invariants in
// docs/architecture/target/v2/camera.zh-Hant.md). After Bevy's bevy_camera: Camera, the target, Projection
// (perspective or orthographic), the Camera2d and Camera3d markers. The derived values (ComputedCamera) are their own
// component, written only by the camera Module (Bevy keeps them inside Camera and writes Projection back). The view
// point is the camera entity's GlobalTransform: the author puts a Transform on it (the kernel lets only a
// component's owner declare its initial value, so Camera cannot bring Transform as Bevy's does).
//
// Conventions (CM-4): projections follow docs/architecture/math.md (left-handed, +Z forward, clip depth 0 at near
// and 1 at far). Viewport positions in the conversions are logical pixels of the target, origin top-left, +Y down,
// including the viewport's offset (as Bevy's).

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/math/linear/VecInt.hpp"
#include "engine/math/scalar/Constants.hpp"
#include "engine/transform/Transform.hpp"

namespace GYO::Engine::Camera {

// A rectangle of the target in physical pixels, origin top-left (Bevy's uses UVec2; a negative position counts as
// 0 when it is clamped into the target).
struct Viewport final {
    Math::Vec2i position{};
    Math::Vec2i size{};
    float minDepth = 0.0F;
    float maxDepth = 1.0F;
};

// The author's settings.
struct Camera final {
    // A larger order is drawn later (Bevy's isize; a fixed width so the hash does not depend on the platform).
    std::int32_t order = 0;
    bool active = true;
    // None: the whole target.
    std::optional<Viewport> viewport;
};

// Where the camera draws: a neutral id. 0 is the main target (the main window when there is one); what other ids
// name is render's and the GPU backend's to map (batches R2a, R2b).
struct CameraTarget final {
    std::uint32_t id = 0;

    friend bool operator==(const CameraTarget&, const CameraTarget&) = default;
};

struct PerspectiveProjection final {
    float verticalFov = Math::Pi / 4.0F;
    float nearClip = 0.1F;
    float farClip = 1000.0F;
};

// One world unit is one logical pixel (times OrthographicProjection::scale).
struct ScalingWindowSize final {};
// The area shown, in world units, whatever the target's size (it stretches).
struct ScalingFixed final {
    float width = 1.0F;
    float height = 1.0F;
};
using ScalingMode = std::variant<ScalingWindowSize, ScalingFixed>;

struct OrthographicProjection final {
    float nearClip = 0.0F;
    float farClip = 1000.0F;
    // Where the camera sits in the area: (0.5, 0.5) its centre, (0, 0) its bottom-left corner.
    Math::Vec2 viewportOrigin{0.5F, 0.5F};
    ScalingMode scalingMode = ScalingWindowSize{};
    float scale = 1.0F;

    // A 2D camera's default: near and far swapped (nearClip 1000, farClip -1000), so a larger z is nearer and is
    // drawn in front, as in Bevy 2D (the user's choice (b), declaration section 13).
    [[nodiscard]] static OrthographicProjection Default2d() noexcept {
        OrthographicProjection projection;
        projection.nearClip = 1000.0F;
        projection.farClip = -1000.0F;
        return projection;
    }
};

// The camera shows a picture of this aspect (width : height) whatever the target's shape: the viewport used
// (ComputedCamera::viewport) is the largest rectangle of this aspect centred in the camera's area (its viewport, or
// the whole target); the rest of the target is left to the clear (black bars above and below, or left and right).
// With ScalingFixed of the same size, a fixed-resolution game (1280 x 720, say) is scaled without
// stretching. The author's Camera::viewport is not changed: it is hashed, and the window's size differs between
// machines. Optional; Bevy has none (an author sets Camera::viewport), Godot's stretch aspect "keep" is the same
// rule. A width or height that is not positive and finite leaves the camera not valid.
struct Letterbox final {
    float width = 16.0F;
    float height = 9.0F;
};

// Bevy's Projection::default is the perspective one.
using Projection = std::variant<PerspectiveProjection, OrthographicProjection>;

// Markers: their initial values give Camera and the Projection of a 2D (orthographic, Default2d) or a 3D
// (perspective) camera. Put one of them, not both.
struct Camera2d final {};
struct Camera3d final {};

// Derived, written only by the camera Module (CM-3).
struct ComputedCamera final {
    // The target size is known and the viewport size is not zero.
    bool valid = false;
    Math::Vec2i targetPhysicalSize{};
    // Physical pixels per logical pixel.
    float scaleFactor = 1.0F;
    // The viewport used (the whole target when Camera has none), clamped into the target.
    Viewport viewport{};
    Math::Matrix4 clipFromView{};
};

struct TargetSize final {
    Math::Vec2i physicalSize{};
    float scaleFactor = 1.0F;
};

// Resource: target id -> its size. A copy of what the window (and render, for textures) own, kept to compute the
// projections and nothing else (CM-5). Written by authors, tests and, later, the glue batch W decides on.
class CameraTargetSizes final {
public:
    void Set(CameraTarget target, TargetSize size);
    void Remove(CameraTarget target);
    [[nodiscard]] const TargetSize* Find(CameraTarget target) const;

private:
    // Ordered by id.
    std::vector<std::pair<std::uint32_t, TargetSize>> sizes_;
};

enum class ViewportConversionFailureCode : std::uint8_t {
    NoViewportSize = 1,
    PastNearPlane,
    PastFarPlane,
    InvalidData,
};

[[nodiscard]] constexpr std::string_view ToString(const ViewportConversionFailureCode code) noexcept {
    switch (code) {
    case ViewportConversionFailureCode::NoViewportSize: return "NoViewportSize";
    case ViewportConversionFailureCode::PastNearPlane: return "PastNearPlane";
    case ViewportConversionFailureCode::PastFarPlane: return "PastFarPlane";
    case ViewportConversionFailureCode::InvalidData: return "InvalidData";
    }
    return "Unknown";
}

// E2: this conversion was not made; everything else is healthy (N25). The message is empty: the code says
// why, and a conversion made every frame (a name tag over a player behind the camera) allocates nothing.
using ViewportConversionFailure = Base::Error<ViewportConversionFailureCode>;
static_assert(Base::CodedError<ViewportConversionFailure>);

// The derived values of one camera, the function UpdateCameras writes ComputedCamera with: the viewport clamped
// into the target, the projection (perspective: the viewport's aspect; orthographic: Bevy's ScalingMode on the
// logical viewport size, scale and viewportOrigin). valid is false when size is null, its scale factor is not
// positive or the viewport size is zero. A pure function: a caller that needs the values of a camera that is not
// in a World (a tool's preview, a test) calls it; it writes nothing.
// letterbox: the camera's Letterbox, or null.
[[nodiscard]] ComputedCamera Compute(const Camera& camera, const Projection& projection, const TargetSize* size,
                                     const Letterbox* letterbox = nullptr);

// Bevy's world_to_viewport: the point's position in the target's logical pixels. NDC depth below 0 is PastNearPlane,
// above 1 PastFarPlane (so a point behind a perspective camera is PastFarPlane, as in Bevy).
[[nodiscard]] Base::Result<Math::Vec2, ViewportConversionFailure> WorldToViewport(
    const ComputedCamera& camera, const Transform::GlobalTransform& view, Math::Vec3 world);
// Bevy's viewport_to_world: the ray from the near plane through the position (direction toward the far plane,
// not normalized).
[[nodiscard]] Base::Result<Math::Ray, ViewportConversionFailure> ViewportToWorld(
    const ComputedCamera& camera, const Transform::GlobalTransform& view, Math::Vec2 position);
// Bevy's viewport_to_world_2d: the position on the near plane, x and y.
[[nodiscard]] Base::Result<Math::Vec2, ViewportConversionFailure> ViewportToWorld2d(
    const ComputedCamera& camera, const Transform::GlobalTransform& view, Math::Vec2 position);

// The author's values, field by field.
inline void HashValue(Kernel::Hasher& h, const Viewport& v) {
    Kernel::HashValue(h, v.position.x);
    Kernel::HashValue(h, v.position.y);
    Kernel::HashValue(h, v.size.x);
    Kernel::HashValue(h, v.size.y);
    Kernel::HashValue(h, v.minDepth);
    Kernel::HashValue(h, v.maxDepth);
}
inline void HashValue(Kernel::Hasher& h, const Camera& v) {
    Kernel::HashValue(h, v.order);
    Kernel::HashValue(h, v.active);
    Kernel::HashValue(h, v.viewport.has_value());
    if (v.viewport) HashValue(h, *v.viewport);
}
inline void HashValue(Kernel::Hasher& h, const CameraTarget& v) { Kernel::HashValue(h, v.id); }
inline void HashValue(Kernel::Hasher& h, const Projection& v) {
    Kernel::HashValue(h, static_cast<std::uint64_t>(v.index()));
    if (const auto* p = std::get_if<PerspectiveProjection>(&v)) {
        Kernel::HashValue(h, p->verticalFov);
        Kernel::HashValue(h, p->nearClip);
        Kernel::HashValue(h, p->farClip);
    } else if (const auto* o = std::get_if<OrthographicProjection>(&v)) {
        Kernel::HashValue(h, o->nearClip);
        Kernel::HashValue(h, o->farClip);
        Kernel::HashValue(h, o->viewportOrigin.x);
        Kernel::HashValue(h, o->viewportOrigin.y);
        Kernel::HashValue(h, static_cast<std::uint64_t>(o->scalingMode.index()));
        if (const auto* f = std::get_if<ScalingFixed>(&o->scalingMode)) {
            Kernel::HashValue(h, f->width);
            Kernel::HashValue(h, f->height);
        }
        Kernel::HashValue(h, o->scale);
    }
}
inline void HashValue(Kernel::Hasher& h, const Letterbox& v) {
    Kernel::HashValue(h, v.width);
    Kernel::HashValue(h, v.height);
}
inline void HashValue(Kernel::Hasher&, const Camera2d&) {}
inline void HashValue(Kernel::Hasher&, const Camera3d&) {}
// Hashes nothing: derived (CM-3).
inline void HashValue(Kernel::Hasher&, const ComputedCamera&) {}
// Hashes nothing: a copy of the window's size, which differs between machines; not state two runs must agree on
// (as net's Peer and AssetView; CM-5).
inline void HashValue(Kernel::Hasher&, const CameraTargetSizes&) {}

} // namespace GYO::Engine::Camera
