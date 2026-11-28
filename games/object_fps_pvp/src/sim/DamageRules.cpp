#include "object_fps_pvp/sim/DamageRules.hpp"

#include "data/JsonFields.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace object_fps_pvp::sim {

namespace {

// The milli-points per metre are a std::uint32_t.
constexpr double kMaximumMilliPointsPerMetre = 4'294'967'295.0;

} // namespace

std::optional<DamageRules> ParseDamageRules(const std::string_view text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the damage rules");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported version");
        DamageRules rules;
        if (data::HasMember(json, "fall_damage")) {
            const auto fall = data::AsObject(data::Member(json, "fall_damage"), "fall_damage");
            FallDamage damage;
            damage.safeHeight = data::Positive(data::Member(fall, "safe_height"), "safe_height");
            const double points = data::Number(data::Member(fall, "points_per_metre"), "points_per_metre");
            const double milli = points * 1000.0;
            if (!std::isfinite(milli) || milli < 0 || milli > kMaximumMilliPointsPerMetre || milli != std::floor(milli)) {
                throw std::runtime_error("points_per_metre is not a whole number of milli-points from 0 to 2^32 - 1");
            }
            damage.milliPointsPerMetre = static_cast<std::uint32_t>(milli);
            rules.fall = damage;
        }
        return rules;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

std::uint32_t LandingDamage(const float gravity, const DamageRules& damage, const float impactSpeed) noexcept {
    if (!damage.fall || !(impactSpeed > 0) || !(gravity > 0)) return 0;
    const double height = static_cast<double>(impactSpeed) * impactSpeed / (2.0 * gravity);
    const double beyond = height - damage.fall->safeHeight;
    if (!(beyond > 0)) return 0;
    const double points = std::floor(beyond * damage.fall->milliPointsPerMetre);
    constexpr std::uint32_t kMost = std::numeric_limits<std::uint32_t>::max();
    return points >= static_cast<double>(kMost) ? kMost : static_cast<std::uint32_t>(points);
}

} // namespace object_fps_pvp::sim
