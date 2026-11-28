#pragma once

#include "object_fps_pvp/present/Overhead.hpp"

#include "engine/camera/Camera.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/physics_3d/Shapes.hpp"
#include "engine/transform/Transform.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace object_fps_pvp::client {

// Frame World: "this thing has a label over it this frame", written by the source System of its kind (the other
// players: TagPlayerOverheads; later a target's or a monster's) in the thing's own units, so the one System that shows
// the labels (ShowOverheads) knows no kind. Presentation only: not decision data, not replicated; it goes with its
// entity.
struct Overhead final {
    // Where the label stands this frame (world, with the clearance above the thing).
    GYO::Engine::Math::Vec3 anchor{};
    // Empty: no name.
    std::string label;
    // The bar's fraction in [0, 1]; none: no bar.
    std::optional<float> health;
    present::OverheadPolicy bar = present::OverheadPolicy::Engaged;
};
void HashValue(GYO::Kernel::Hasher&, const Overhead&);

// Frame World: one piece of the label of target on the HUD's layer: its name (a Sprite) or its bar (a Shape2d).
struct OverheadPart final {
    GYO::Kernel::EntityId target = 0;
    bool bar = false;
};
void HashValue(GYO::Kernel::Hasher&, const OverheadPart&);

// Where an overhead label goes on the HUD's 2D layer this frame, or why it does not show.
enum class OverheadHidden : std::uint8_t {
    NoView,     // the world camera has no picture to place it in (its target's size is not known yet)
    Behind,     // behind the camera, or nearer than its near plane (camera's PastFarPlane, PastNearPlane)
    OffScreen,  // outside the world camera's viewport
    Occluded,   // the walls stand between the camera and the anchor
};
using OverheadPlacement = std::variant<GYO::Engine::Math::Vec2, OverheadHidden>;

// The world camera (its derived values and where it is) and the HUD camera: the anchor's point on the world camera's
// picture (camera's WorldToViewport, Bevy's world_to_viewport), then that point on the HUD camera's 2D plane
// (ViewportToWorld2d, Bevy's viewport_to_world_2d). The cheap reasons first; the walls (none: nothing stands between)
// last. There is no distance limit: what the world camera draws gets its label.
[[nodiscard]] OverheadPlacement PlaceOverhead(const GYO::Engine::Camera::ComputedCamera& world,
                                              const GYO::Engine::Transform::GlobalTransform& worldView,
                                              const GYO::Engine::Camera::ComputedCamera& hud,
                                              const GYO::Engine::Transform::GlobalTransform& hudView, GYO::Engine::Math::Vec3 anchor,
                                              const GYO::Engine::Physics3d::BoxSet* walls);

} // namespace object_fps_pvp::client
