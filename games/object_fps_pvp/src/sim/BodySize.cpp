#include "object_fps_pvp/sim/BodySize.hpp"

#include "data/JsonFields.hpp"

#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::sim {

std::optional<BodySize> ParseBodySize(const std::string_view characterText, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(characterText);
        const auto character = data::AsObject(document.Root(), "the character");
        BodySize body;
        if (!data::HasMember(character, "body")) return body;
        const auto json = data::AsObject(data::Member(character, "body"), "body");
        body.height = data::PositiveOr(json, "height", body.height);
        body.radius = data::PositiveOr(json, "radius", body.radius);
        body.eyeHeight = data::PositiveOr(json, "eye_height", body.eyeHeight);
        body.crouchHeight = data::PositiveOr(json, "crouch_height", body.crouchHeight);
        body.crouchEyeHeight = data::PositiveOr(json, "crouch_eye_height", body.crouchEyeHeight);
        if (body.eyeHeight > body.height || body.crouchEyeHeight > body.crouchHeight) throw std::runtime_error("an eye is above the top of its body");
        if (!(body.crouchHeight < body.height)) throw std::runtime_error("the crouched body is not shorter than the standing one");
        if (body.crouchHeight < 2 * body.radius) throw std::runtime_error("a body is shorter than its two caps");
        return body;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

} // namespace object_fps_pvp::sim
