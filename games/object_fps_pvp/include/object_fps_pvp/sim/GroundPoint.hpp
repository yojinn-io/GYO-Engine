#pragma once

namespace object_fps_pvp::sim {

// A point or offset on the XZ ground plane, in world units. This is a game
// semantic type, not a duplicate of Math::Vec2: Vec2{x, y} would map z to y
// silently. Lift to 3D explicitly as Math::Vec3{p.x, y, p.z}.
struct GroundPoint final {
    float x = 0.0f;
    float z = 0.0f;
};

} // namespace object_fps_pvp::sim
