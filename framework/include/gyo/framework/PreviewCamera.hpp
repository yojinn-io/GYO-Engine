#pragma once

#include "engine/math/linear/Vec3.hpp"
#include "engine/render/RenderTypes.hpp"

namespace Gyo::Tools::Framework {

// A camera turning about a point: yaw about the vertical, then pitch (positive
// looks down), at distance from target. Camera() gives it in Engine's camera
// convention: the camera's rotation is Z, then Y, then X Euler angles, +Z
// forward; near and far clip follow the distance.
struct OrbitCamera final {
    Engine::Math::Vec3 target{};
    float yawRadians{};
    float pitchRadians{0.3F};
    float distance{3.0F};
    float verticalFieldOfViewRadians{0.9F};

    [[nodiscard]] Engine::Render::PerspectiveCamera3D Camera() const noexcept;
};

} // namespace Gyo::Tools::Framework
