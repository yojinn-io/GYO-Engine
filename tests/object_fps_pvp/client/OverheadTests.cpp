// The overhead labels' placement (client/Overhead.hpp): where the anchor of a thing goes on the HUD's 2D layer, or why
// it does not show. Pure: the cameras' derived values are computed here as the camera Module computes them.
#include <doctest/doctest.h>

#include "object_fps_pvp/client/Overhead.hpp"

#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <variant>
#include <vector>

namespace {
namespace C = GYO::Engine::Camera;
namespace Math = GYO::Engine::Math;
namespace P = GYO::Engine::Physics3d;
namespace T = GYO::Engine::Transform;
using object_fps_pvp::client::OverheadHidden;
using object_fps_pvp::client::PlaceOverhead;

// The Client's cameras (ClientWorld.cpp): the world camera (fov 1.2, near 0.05, far 150) and the HUD camera (2D,
// 1280 x 720 units, the origin at the centre, y up), on a 1280 x 720 window.
constexpr float kFov = 1.2F;
const C::TargetSize kWindow{{1280, 720}, 1.0F};

C::ComputedCamera WorldCamera() {
    C::PerspectiveProjection perspective;
    perspective.verticalFov = kFov;
    perspective.nearClip = 0.05F;
    perspective.farClip = 150.0F;
    return C::Compute(C::Camera{}, C::Projection{perspective}, &kWindow);
}

C::ComputedCamera HudCamera() {
    C::OrthographicProjection flat = C::OrthographicProjection::Default2d();
    flat.scalingMode = C::ScalingFixed{1280.0F, 720.0F};
    return C::Compute(C::Camera{}, C::Projection{flat}, &kWindow);
}

// The world camera at eye, looking along +Z (yaw 0).
T::GlobalTransform At(const Math::Vec3 eye) {
    T::Transform t;
    t.translation = eye;
    return T::GlobalTransform{T::ToMatrix(t)};
}

const T::GlobalTransform kHudView{T::ToMatrix(T::Transform{})};

Math::Vec2 Shown(const object_fps_pvp::client::OverheadPlacement& placement) {
    REQUIRE_MESSAGE(std::holds_alternative<Math::Vec2>(placement), "shown");
    return std::get<Math::Vec2>(placement);
}

OverheadHidden Hidden(const object_fps_pvp::client::OverheadPlacement& placement) {
    REQUIRE_MESSAGE(std::holds_alternative<OverheadHidden>(placement), "hidden");
    return std::get<OverheadHidden>(placement);
}

P::BoxSet Walls(std::vector<Math::Aabb> boxes) {
    auto built = P::BoxSet::Build(boxes);
    REQUIRE(static_cast<bool>(built));
    return std::move(built).value();
}
} // namespace

TEST_CASE("client: overhead: the anchor's place on the HUD is the world camera's picture of it") {
    const C::ComputedCamera world = WorldCamera();
    const C::ComputedCamera hud = HudCamera();
    REQUIRE(world.valid);
    REQUIRE(hud.valid);
    const Math::Vec3 eye{2.0F, 1.6F, -3.0F};
    const T::GlobalTransform view = At(eye);
    const float halfHeight = std::tan(kFov / 2.0F);
    const float halfWidth = halfHeight * 1280.0F / 720.0F;
    // Straight ahead: the centre.
    const Math::Vec2 ahead = Shown(PlaceOverhead(world, view, hud, kHudView, eye + Math::Vec3{0.0F, 0.0F, 10.0F}, nullptr));
    CHECK_MESSAGE(ahead.x == doctest::Approx(0.0F).epsilon(0.01), "[pvp-overhead-place]");
    CHECK(ahead.y == doctest::Approx(0.0F).epsilon(0.01));
    // To the right (+X) by an angle: x by the field of view, y unchanged.
    const float angle = 0.3F;
    const Math::Vec2 right =
        Shown(PlaceOverhead(world, view, hud, kHudView, eye + Math::Vec3{10.0F * std::tan(angle), 0.0F, 10.0F}, nullptr));
    CHECK_MESSAGE(right.x == doctest::Approx(std::tan(angle) / halfWidth * 640.0F).epsilon(0.001), "[pvp-overhead-place-x]");
    CHECK(std::abs(right.y) < 0.01F);
    // Above: y up on the HUD (the viewport's y runs down).
    const Math::Vec2 above = Shown(PlaceOverhead(world, view, hud, kHudView, eye + Math::Vec3{0.0F, 1.0F, 10.0F}, nullptr));
    CHECK_MESSAGE(above.y == doctest::Approx(0.1F / halfHeight * 360.0F).epsilon(0.001), "[pvp-overhead-place-y]");
    CHECK(std::abs(above.x) < 0.01F);
    // Both ways, as Bevy's 2d_viewport_to_world example checks: the ray back through the anchor's viewport point meets
    // the anchor.
    const Math::Vec3 anchor = eye + Math::Vec3{-1.5F, 0.7F, 6.0F};
    const auto point = C::WorldToViewport(world, view, anchor);
    REQUIRE(static_cast<bool>(point));
    const auto ray = C::ViewportToWorld(world, view, *point);
    REQUIRE(static_cast<bool>(ray));
    const Math::Vec3 along = anchor - ray->origin;
    const Math::Vec3 direction = ray->direction * (1.0F / Math::Length(ray->direction));
    const Math::Vec3 off = along - direction * Math::Dot(along, direction);
    CHECK(Math::Length(off) < 1e-3F);
    const Math::Vec2 placed = Shown(PlaceOverhead(world, view, hud, kHudView, anchor, nullptr));
    CHECK(placed.x == doctest::Approx(point->x - 640.0F).epsilon(0.001));
    CHECK(placed.y == doctest::Approx(360.0F - point->y).epsilon(0.001));
}

TEST_CASE("client: overhead: behind the camera, off the picture, or behind a wall, a label does not show; far is shown") {
    const C::ComputedCamera world = WorldCamera();
    const C::ComputedCamera hud = HudCamera();
    const Math::Vec3 eye{0.0F, 1.6F, 0.0F};
    const T::GlobalTransform view = At(eye);
    CHECK_MESSAGE(Hidden(PlaceOverhead(world, view, hud, kHudView, eye + Math::Vec3{0.0F, 0.0F, -5.0F}, nullptr)) == OverheadHidden::Behind,
                  "[pvp-overhead-behind]");
    CHECK(Hidden(PlaceOverhead(world, view, hud, kHudView, eye + Math::Vec3{0.0F, 0.0F, 0.01F}, nullptr)) == OverheadHidden::Behind);
    CHECK_MESSAGE(Hidden(PlaceOverhead(world, view, hud, kHudView, eye + Math::Vec3{20.0F, 0.0F, 5.0F}, nullptr)) ==
                      OverheadHidden::OffScreen,
                  "[pvp-overhead-offscreen]");
    CHECK(Hidden(PlaceOverhead(world, view, hud, kHudView, eye + Math::Vec3{0.0F, -20.0F, 5.0F}, nullptr)) == OverheadHidden::OffScreen);
    // A wall across the way at z 4..5; the anchor at z 10.
    const P::BoxSet wall = Walls({{{-3.0F, 0.0F, 4.0F}, {3.0F, 3.0F, 5.0F}}});
    const Math::Vec3 behindWall = eye + Math::Vec3{0.0F, 0.5F, 10.0F};
    CHECK_MESSAGE(Hidden(PlaceOverhead(world, view, hud, kHudView, behindWall, &wall)) == OverheadHidden::Occluded, "[pvp-overhead-occluded]");
    // The wall moved aside: shown.
    const P::BoxSet aside = Walls({{{10.0F, 0.0F, 4.0F}, {13.0F, 3.0F, 5.0F}}});
    Shown(PlaceOverhead(world, view, hud, kHudView, behindWall, &aside));
    // An anchor against the wall's near face is not behind it.
    Shown(PlaceOverhead(world, view, hud, kHudView, Math::Vec3{0.0F, 2.0F, 4.0F}, &wall));
    // No distance limit: 140 m away (inside the far plane) with nothing between, shown.
    const Math::Vec2 far = Shown(PlaceOverhead(world, view, hud, kHudView, eye + Math::Vec3{0.0F, 0.0F, 140.0F}, &aside));
    CHECK_MESSAGE(std::abs(far.x) < 0.01F, "[pvp-overhead-no-distance-limit]");
    // A camera without a picture yet (no window size): nothing to place it in.
    const C::ComputedCamera none = C::Compute(C::Camera{}, C::Projection{C::PerspectiveProjection{}}, nullptr);
    CHECK(Hidden(PlaceOverhead(none, view, hud, kHudView, behindWall, nullptr)) == OverheadHidden::NoView);
}
