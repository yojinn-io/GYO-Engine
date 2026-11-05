#include "gyo/framework/PreviewCamera.hpp"

#include "engine/math/linear/Matrix4.hpp"

#include <algorithm>

namespace Gyo::Tools::Framework {

Engine::Render::PerspectiveCamera3D OrbitCamera::Camera() const noexcept {
    namespace Math = Engine::Math;
    // Engine's camera: rotation Z, then Y, then X; +Z forward. MakeRotationX
    // takes +Z to (0, -sin, cos), so a positive pitch looks down.
    const Math::Matrix4 rotation = Math::Multiply(Math::MakeRotationY(yawRadians), Math::MakeRotationX(pitchRadians));
    const Math::Vec3 forward = Math::TransformPoint(rotation, {0, 0, 1});
    Engine::Render::PerspectiveCamera3D camera;
    camera.position = target - forward * distance;
    camera.rotationRadians = {pitchRadians, yawRadians, 0};
    camera.verticalFieldOfViewRadians = verticalFieldOfViewRadians;
    camera.nearClip = std::max(distance * 0.01F, 0.001F);
    camera.farClip = std::max(distance * 10.0F, 1.0F);
    return camera;
}

} // namespace Gyo::Tools::Framework
