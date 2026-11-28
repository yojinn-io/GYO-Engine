#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::sim {

// The character's body (characters/player/character.json, version 2, its body): the standing and the crouched
// capsule's height, its radius, and the eye's height in each stance. The one source of the body for collision, shots,
// the camera and the character's scale.
struct BodySize final {
    float height = 1.8F;
    float radius = 0.25F;
    float eyeHeight = 1.6F;
    float crouchHeight = 1.2F;
    float crouchEyeHeight = 1.0F;
    friend bool operator==(const BodySize&, const BodySize&) = default;
};

// Reads the body of a character file (its "body" member; absent, or a member of it absent: the defaults). nullopt
// with error when the file does not parse, body is not an object, a member is not a finite positive number, an eye is
// above the top of its body, the crouched body is not shorter than the standing one, or a body is shorter than its two
// caps (2 x radius).
[[nodiscard]] std::optional<BodySize> ParseBodySize(std::string_view characterText, std::string& error);

[[nodiscard]] constexpr float BodyHeight(const BodySize& body, const bool crouched) noexcept {
    return crouched ? body.crouchHeight : body.height;
}
[[nodiscard]] constexpr float EyeHeight(const BodySize& body, const bool crouched) noexcept {
    return crouched ? body.crouchEyeHeight : body.eyeHeight;
}

} // namespace object_fps_pvp::sim
