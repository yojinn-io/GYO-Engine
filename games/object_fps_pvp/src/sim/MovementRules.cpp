#include "object_fps_pvp/sim/MovementRules.hpp"

#include "data/JsonFields.hpp"

#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::sim {

std::optional<MovementRules> ParseMovementRules(const std::string_view text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the movement rules");
        if (data::Integer(data::Member(json, "version"), "version") != 3) throw std::runtime_error("unsupported version");
        MovementRules rules;
        rules.movementSpeed = data::PositiveOr(json, "movement_speed", rules.movementSpeed);
        rules.jumpHeight = data::PositiveOr(json, "jump_height", rules.jumpHeight);
        rules.gravity = data::PositiveOr(json, "gravity", rules.gravity);
        if (!std::isfinite(2 * rules.gravity * rules.jumpHeight)) throw std::runtime_error("the jump speed is not finite");
        rules.crouchSpeedScale = data::Positive(data::Member(json, "crouch_speed_scale"), "crouch_speed_scale");
        rules.sprintSpeedScale = data::Positive(data::Member(json, "sprint_speed_scale"), "sprint_speed_scale");
        if (rules.crouchSpeedScale > 1) throw std::runtime_error("crouch_speed_scale is above 1");
        if (rules.sprintSpeedScale > 3) throw std::runtime_error("sprint_speed_scale is above 3");
        rules.holdJump = data::Boolean(data::Member(json, "hold_jump"), "hold_jump");
        return rules;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

float MoveSpeed(const MovementRules& rules, const bool crouched) noexcept {
    return crouched ? rules.movementSpeed * rules.crouchSpeedScale : rules.movementSpeed;
}

float JumpSpeed(const MovementRules& rules) noexcept { return std::sqrt(2 * rules.gravity * rules.jumpHeight); }

VerticalStep Fall(const float y, const float velocity, const float gravity, const double seconds, const std::optional<float> floor) noexcept {
    VerticalStep step;
    step.y = static_cast<float>(y + velocity * seconds - 0.5 * gravity * seconds * seconds);
    step.velocity = static_cast<float>(velocity - gravity * seconds);
    if (floor && step.y <= *floor) {
        step.y = *floor;
        step.impact = step.velocity < 0 ? -step.velocity : 0.0F;
        step.velocity = 0;
    }
    return step;
}

} // namespace object_fps_pvp::sim
