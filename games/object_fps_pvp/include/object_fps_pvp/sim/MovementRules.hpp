#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::sim {

// The player's movement rules (rules/movement_rules.json, version 3; a mode's "movement" rules): how fast a body moves
// (and, crouched or sprinting, at what scale of it), how high it jumps under what gravity, and whether holding the
// jump key jumps again on every landing (hold_jump). The body's shape is the character's (sim/BodySize.hpp).
struct MovementRules final {
    float movementSpeed = 3.0F;
    float jumpHeight = 0.6F;
    float gravity = 18.0F;
    float crouchSpeedScale = 0.5F;
    float sprintSpeedScale = 1.0F;
    bool holdJump = false;
    friend bool operator==(const MovementRules&, const MovementRules&) = default;
};

// Reads the movement rules. nullopt with error: a member missing (crouch_speed_scale, sprint_speed_scale, hold_jump)
// or of the wrong kind, not version 3, movement_speed, jump_height or gravity (each optional) not a finite positive
// number or a jump speed that is not finite, a crouch speed scale not in (0, 1] or a sprint speed scale not in (0, 3].
[[nodiscard]] std::optional<MovementRules> ParseMovementRules(std::string_view text, std::string& error);

[[nodiscard]] float MoveSpeed(const MovementRules& rules, bool crouched) noexcept;

// Whether a step asks to jump: the key pressed since the last step, or, with hold_jump, held (a body on the ground then
// jumps again as soon as it lands). The step's input carries the answer as its jump press (PlayerInput::jump).
[[nodiscard]] constexpr bool StepJumps(const bool pressed, const bool held, const MovementRules& rules) noexcept {
    return pressed || (rules.holdJump && held);
}

// The upward speed that reaches the jump height under the gravity: sqrt(2 g h).
[[nodiscard]] float JumpSpeed(const MovementRules& rules) noexcept;

// One step of the fall: the analytic constant acceleration (y + v t - g t^2 / 2, v - g t), so the jump reaches
// exactly the configured height whatever the step length. A floor plane (when the arena has one) stops it; impact is
// the downward speed it is stopped at (0 when it is not stopped).
struct VerticalStep final {
    float y = 0.0F;
    float velocity = 0.0F;
    float impact = 0.0F;
};
[[nodiscard]] VerticalStep Fall(float y, float velocity, float gravity, double seconds, std::optional<float> floor) noexcept;

} // namespace object_fps_pvp::sim
