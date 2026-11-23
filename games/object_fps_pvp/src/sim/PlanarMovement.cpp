#include "object_fps_pvp/sim/PlanarMovement.hpp"

#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <cmath>

namespace object_fps_pvp::sim {

namespace Math = GYO::Engine::Math;

GroundPoint ComputePlanarInput(
    const float forwardAxis,
    const float rightAxis,
    const float yawRadians,
    const float pitchRadians) noexcept {
    static_cast<void>(pitchRadians);
    const float clampedForward = Math::Clamp(forwardAxis, -1.0f, 1.0f);
    const float clampedRight = Math::Clamp(rightAxis, -1.0f, 1.0f);
    const float sinYaw = std::sin(yawRadians);
    const float cosYaw = std::cos(yawRadians);

    // The ground vector lifted to y = 0, so Math's vector operations apply
    // without remapping z.
    Math::Vec3 movement{
        sinYaw * clampedForward + cosYaw * clampedRight,
        0.0f,
        cosYaw * clampedForward - sinYaw * clampedRight,
    };

    if (Math::LengthSquared(movement) > 1.0f) {
        movement = Math::Normalize(movement);
    }

    return {movement.x, movement.z};
}

GroundPoint ComputePlanarDisplacement(
    const float forwardAxis, const float rightAxis, const float yawRadians,
    const float movementSpeed, const float deltaSeconds) noexcept {
    const GroundPoint direction = ComputePlanarInput(forwardAxis, rightAxis, yawRadians);
    return {direction.x * movementSpeed * deltaSeconds,
            direction.z * movementSpeed * deltaSeconds};
}

} // namespace object_fps_pvp::sim
