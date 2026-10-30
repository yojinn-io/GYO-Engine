#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Gameplay/Player/PlanarMovement.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Constants.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <stdexcept>

namespace fps::pvp {

bool ValidMovementCommand(const MovementCommand& command) noexcept {
    return command.sequence != 0 &&
        std::isfinite(command.moveForward) && std::abs(command.moveForward) <= 1 &&
        std::isfinite(command.moveRight) && std::abs(command.moveRight) <= 1 &&
        std::isfinite(command.yaw) && std::abs(command.yaw) <= 1.0e6F &&
        std::isfinite(command.pitch) && std::abs(command.pitch) <= Engine::Math::HalfPi;
}

bool ValidStanceRules(const Arena& arena, const StanceRules& rules) noexcept {
    const auto finite = [](float value) { return std::isfinite(value); };
    return finite(rules.crouchBodyHeight) && finite(rules.crouchEyeHeight) &&
        finite(rules.crouchSpeedScale) && finite(rules.sprintSpeedScale) &&
        rules.crouchEyeHeight > 0 && rules.crouchEyeHeight <= rules.crouchBodyHeight &&
        rules.crouchBodyHeight < arena.bodyHeight && rules.crouchBodyHeight >= 2 * arena.radius &&
        rules.crouchSpeedScale > 0 && rules.crouchSpeedScale <= 1 &&
        rules.sprintSpeedScale > 0 && rules.sprintSpeedScale <= 3;
}

float StanceBodyHeight(const Arena& arena, const StanceRules& rules, bool crouched) noexcept {
    return crouched ? rules.crouchBodyHeight : arena.bodyHeight;
}

float StanceEyeHeight(const Arena& arena, const StanceRules& rules, bool crouched) noexcept {
    return crouched ? rules.crouchEyeHeight : arena.eyeHeight;
}

PlayerState StepMovement(const Arena& arena, const PlayerState& state, const MovementCommand& command) {
    return StepMovement(arena, DefaultStanceRules, state, command);
}

PlayerState StepMovement(const Arena& arena, const StanceRules& rules,
    const PlayerState& state, const MovementCommand& command) {
    if (!ValidMovementCommand(command)) throw std::invalid_argument("Invalid movement command");
    auto result = state;
    const bool alive = state.lifeState == LifeState::Alive;
    if (alive) {
        result.yaw = Engine::Math::WrapRadians(command.yaw);
        result.pitch = Engine::Math::Clamp(command.pitch, -MovementMaximumPitch, MovementMaximumPitch);
    }
    const auto p = state.position;
    // Stance (protocol-v8 §4): alive steps follow the held crouch on the
    // ground and in the air, except that a jump from the ground targets the
    // standing stance. Standing up needs room at the current feet; the
    // initial-penetration push of MoveCharacterBody must never stand a body up.
    bool crouched = state.crouched;
    if (alive) {
        const bool jumpFromGround = command.jumpRequested && state.grounded;
        if (command.crouchHeld && !jumpFromGround) {
            crouched = true;
        } else if (crouched && CanPlaceCharacterBody({p, arena.bodyHeight, arena.radius}, arena.walls, {})) {
            crouched = false;
        }
    }
    result.crouched = crouched;
    result.sprinting = false; // The sprint behaviour arrives with v8-product-sprint.
    const float bodyHeight = StanceBodyHeight(arena, rules, crouched);
    const float speed = crouched ? arena.movementSpeed * rules.crouchSpeedScale : arena.movementSpeed;
    const auto displacement = ComputePlanarDisplacement(alive ? command.moveForward : 0,
        alive ? command.moveRight : 0,
        result.yaw, speed, static_cast<float>(MovementTickSeconds));
    const auto supported = [&](const Engine::Math::Vec3& feet) {
        if (feet.y <= 0.0001F) return true;
        const Engine::Collision::VerticalCapsule body{feet, bodyHeight, arena.radius};
        for (const auto& wall : arena.walls) {
            const auto contact = Engine::Collision::SweepVerticalCapsuleAgainstAabb(
                body, {0, -0.002F, 0}, wall);
            if (contact && contact->normal.y > 0.5F) return true;
        }
        return false;
    };
    if (result.grounded) result.grounded = supported(p);
    if (alive && command.jumpRequested && result.grounded && !crouched) {
        result.verticalVelocity = std::sqrt(2 * arena.gravity * arena.jumpHeight);
        result.grounded = false;
    }
    float desiredY = p.y;
    if (!result.grounded) {
        // Analytic constant acceleration preserves the configured jump height.
        desiredY = static_cast<float>(p.y + result.verticalVelocity * MovementTickSeconds -
            0.5 * arena.gravity * MovementTickSeconds * MovementTickSeconds);
        result.verticalVelocity -= static_cast<float>(arena.gravity * MovementTickSeconds);
        if (desiredY <= 0) {
            desiredY = 0;
            result.verticalVelocity = 0;
        }
    }
    result.position = MoveCharacterBody({p, bodyHeight, arena.radius},
        {displacement.x, desiredY - p.y, displacement.z}, arena.walls, {}, false);
    if (std::abs(result.position.y - desiredY) > 0.0001F) result.verticalVelocity = 0;
    result.grounded = result.verticalVelocity <= 0 && supported(result.position);
    if (result.grounded) result.verticalVelocity = 0;
    result.lastResolvedCommand = command.sequence;
    return result;
}

} // namespace fps::pvp
