#include "object_fps_pvp/sim/MovementRules.hpp"

#include "data/JsonFields.hpp"

#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::sim {

std::optional<StanceRules> ParseStanceRules(const std::string_view text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the movement rules");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported version");
        StanceRules rules;
        const auto number = [&](const std::string_view key) {
            const auto value = static_cast<float>(data::Number(data::Member(json, key), key));
            if (!std::isfinite(value)) throw std::runtime_error(std::string{key} + " is not finite");
            return value;
        };
        rules.crouchBodyHeight = number("crouch_body_height");
        rules.crouchEyeHeight = number("crouch_eye_height");
        rules.crouchSpeedScale = number("crouch_speed_scale");
        rules.sprintSpeedScale = number("sprint_speed_scale");
        return rules;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

bool StanceFits(const ArenaContent& arena, const StanceRules& rules) noexcept {
    const auto finite = [](const float v) { return std::isfinite(v); };
    return finite(rules.crouchBodyHeight) && finite(rules.crouchEyeHeight) && finite(rules.crouchSpeedScale) &&
           finite(rules.sprintSpeedScale) && rules.crouchEyeHeight > 0 && rules.crouchEyeHeight <= rules.crouchBodyHeight &&
           rules.crouchBodyHeight < arena.bodyHeight && rules.crouchBodyHeight >= 2 * arena.radius &&
           rules.crouchSpeedScale > 0 && rules.crouchSpeedScale <= 1 && rules.sprintSpeedScale > 0 &&
           rules.sprintSpeedScale <= 3;
}

float BodyHeight(const ArenaContent& arena, const StanceRules& rules, const bool crouched) noexcept {
    return crouched ? rules.crouchBodyHeight : arena.bodyHeight;
}

float EyeHeight(const ArenaContent& arena, const StanceRules& rules, const bool crouched) noexcept {
    return crouched ? rules.crouchEyeHeight : arena.eyeHeight;
}

float MoveSpeed(const ArenaContent& arena, const StanceRules& rules, const bool crouched) noexcept {
    return crouched ? arena.movementSpeed * rules.crouchSpeedScale : arena.movementSpeed;
}

float JumpSpeed(const ArenaContent& arena) noexcept { return std::sqrt(2 * arena.gravity * arena.jumpHeight); }

VerticalStep Fall(const float y, const float velocity, const float gravity, const double seconds) noexcept {
    VerticalStep step;
    step.y = static_cast<float>(y + velocity * seconds - 0.5 * gravity * seconds * seconds);
    step.velocity = static_cast<float>(velocity - gravity * seconds);
    if (step.y <= 0) {
        step.y = 0;
        step.velocity = 0;
    }
    return step;
}

} // namespace object_fps_pvp::sim
