#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace object_fps_pvp::sim {

// How a landing hurts: a fall from higher than the safe height takes the points per metre beyond it.
struct FallDamage final {
    float safeHeight = 0.0F;
    // Milli-points (as HP) per metre fallen beyond the safe height; the file gives points.
    std::uint32_t milliPointsPerMetre = 0;
    friend bool operator==(const FallDamage&, const FallDamage&) = default;
};

// The damage rules (rules/damage_rules.json, version 1; a mode's "damage" rules, optional): for now how a landing
// hurts (fall_damage; absent: a landing never hurts). A mode without damage rules plays with none of it.
struct DamageRules final {
    std::optional<FallDamage> fall;
    friend bool operator==(const DamageRules&, const DamageRules&) = default;
};

// Reads the damage rules. nullopt with error: not version 1, fall_damage not an object, its safe height not a finite
// positive number, or its points per metre not a whole number of milli-points (0 to 2^32 - 1).
[[nodiscard]] std::optional<DamageRules> ParseDamageRules(std::string_view text, std::string& error);

// The damage of a landing at a downward speed, in milli-points: the height such a speed falls from under the gravity
// (the movement rules': v^2 / 2g), beyond the safe height, times the points per metre (rounded down); 0 without fall
// damage.
[[nodiscard]] std::uint32_t LandingDamage(float gravity, const DamageRules& damage, float impactSpeed) noexcept;

} // namespace object_fps_pvp::sim
