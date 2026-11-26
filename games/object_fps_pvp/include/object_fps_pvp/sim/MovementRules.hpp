#pragma once

#include "object_fps_pvp/sim/Arena.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::sim {

// The player's movement rules (rules/movement_rules.json, version 2): the crouched stance (standing takes the arena's
// body and eye heights), and whether holding the jump key jumps again on every landing (hold_jump).
struct StanceRules final {
    float crouchBodyHeight = 1.2F;
    float crouchEyeHeight = 1.0F;
    float crouchSpeedScale = 0.5F;
    float sprintSpeedScale = 1.0F;
    bool holdJump = false;
    friend bool operator==(const StanceRules&, const StanceRules&) = default;
};

[[nodiscard]] std::optional<StanceRules> ParseStanceRules(std::string_view text, std::string& error);

// The rules fit the arena: finite, 0 < crouched eye <= crouched body < standing body, the crouched body not shorter
// than its two caps, 0 < crouch speed scale <= 1, 0 < sprint speed scale <= 3.
[[nodiscard]] bool StanceFits(const ArenaContent& arena, const StanceRules& rules) noexcept;

// The one source of the body and eye heights for collision, shots and the camera.
[[nodiscard]] float BodyHeight(const ArenaContent& arena, const StanceRules& rules, bool crouched) noexcept;
[[nodiscard]] float EyeHeight(const ArenaContent& arena, const StanceRules& rules, bool crouched) noexcept;
[[nodiscard]] float MoveSpeed(const ArenaContent& arena, const StanceRules& rules, bool crouched) noexcept;

// Whether a step asks to jump: the key pressed since the last step, or, with hold_jump, held (a body on the ground then
// jumps again as soon as it lands). The step's input carries the answer as its jump press (PlayerInput::jump).
[[nodiscard]] constexpr bool StepJumps(const bool pressed, const bool held, const StanceRules& rules) noexcept {
    return pressed || (rules.holdJump && held);
}

// The upward speed that reaches the arena's jump height under its gravity: sqrt(2 g h).
[[nodiscard]] float JumpSpeed(const ArenaContent& arena) noexcept;

// One step of the fall: the analytic constant acceleration (y + v t - g t^2 / 2, v - g t), so the jump reaches
// exactly the configured height whatever the step length. A floor plane (when the arena has one) stops it; impact is
// the downward speed it is stopped at (0 when it is not stopped).
struct VerticalStep final {
    float y = 0.0F;
    float velocity = 0.0F;
    float impact = 0.0F;
};
[[nodiscard]] VerticalStep Fall(float y, float velocity, float gravity, double seconds, std::optional<float> floor) noexcept;

// The damage of a landing at a downward speed, in milli-points: the height such a speed falls from under the arena's
// gravity (v^2 / 2g), beyond the safe height, times the points per metre (rounded down); 0 without fall damage.
[[nodiscard]] std::uint32_t LandingDamage(const ArenaContent& arena, float impactSpeed) noexcept;

// Whether a body whose feet are at this height has fallen out of the arena (below its kill height).
[[nodiscard]] constexpr bool BelowKillHeight(const ArenaContent& arena, const float feetY) noexcept { return feetY < arena.killHeight; }

} // namespace object_fps_pvp::sim
