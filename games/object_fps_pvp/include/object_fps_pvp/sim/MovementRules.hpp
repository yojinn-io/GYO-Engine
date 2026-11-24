#pragma once

#include "object_fps_pvp/sim/Arena.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::sim {

// The crouched stance (rules/movement_rules.json, version 1). Standing takes the arena's body and eye heights.
struct StanceRules final {
    float crouchBodyHeight = 1.2F;
    float crouchEyeHeight = 1.0F;
    float crouchSpeedScale = 0.5F;
    float sprintSpeedScale = 1.0F;
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

// The upward speed that reaches the arena's jump height under its gravity: sqrt(2 g h).
[[nodiscard]] float JumpSpeed(const ArenaContent& arena) noexcept;

// One step of the fall: the analytic constant acceleration (y + v t - g t^2 / 2, v - g t), so the jump reaches
// exactly the configured height whatever the step length. The floor (y = 0) stops it.
struct VerticalStep final {
    float y = 0.0F;
    float velocity = 0.0F;
};
[[nodiscard]] VerticalStep Fall(float y, float velocity, float gravity, double seconds) noexcept;

} // namespace object_fps_pvp::sim
