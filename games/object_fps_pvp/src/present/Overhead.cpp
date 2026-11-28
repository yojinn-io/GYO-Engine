#include "object_fps_pvp/present/Overhead.hpp"

#include "object_fps_pvp/sim/CombatRules.hpp"
#include "data/JsonFields.hpp"

#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::present {

namespace Json = GYO::Engine::Json;

namespace {

std::array<float, 3> Color(const Json::Value& value, const std::string& what) {
    const auto array = value.AsArray();
    if (!array || array.value().Size() != 3) throw std::runtime_error(what + " is three components");
    std::array<float, 3> color{};
    std::size_t i = 0;
    for (const auto component : array.value()) {
        color[i] = static_cast<float>(data::Number(component, what));
        if (!std::isfinite(color[i]) || color[i] < 0 || color[i] > 1) throw std::runtime_error(what + "'s components are in [0, 1]");
        ++i;
    }
    return color;
}

float Positive(const Json::ObjectView& object, const std::string& key) {
    const auto number = static_cast<float>(data::Number(data::Member(object, key), key));
    if (!std::isfinite(number) || number <= 0) throw std::runtime_error(key + " is positive");
    return number;
}

double Seconds(const Json::ObjectView& object, const std::string& key) {
    const double seconds = data::Number(data::Member(object, key), key);
    if (!std::isfinite(seconds) || seconds < 0) throw std::runtime_error(key + " is seconds, not negative");
    return seconds;
}

float Fraction(const Json::ObjectView& object, const std::string& key, const bool openAbove) {
    const auto number = static_cast<float>(data::Number(data::Member(object, key), key));
    if (!std::isfinite(number) || number <= 0 || (openAbove ? number >= 1 : number > 1)) {
        throw std::runtime_error(key + (openAbove ? " is in (0, 1)" : " is in (0, 1]"));
    }
    return number;
}

} // namespace

std::optional<OverheadSettings> ParseOverheadSettings(const std::string& text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the settings");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported version");
        OverheadSettings settings;
        settings.headClearanceMetres = static_cast<float>(data::Number(data::Member(json, "head_clearance_metres"), "head_clearance_metres"));
        if (!std::isfinite(settings.headClearanceMetres) || settings.headClearanceMetres < 0) {
            throw std::runtime_error("head_clearance_metres is not negative");
        }
        const auto name = data::AsObject(data::Member(json, "name"), "name");
        settings.textPixels = Positive(name, "text_pixels");
        settings.nameColor = Color(data::Member(name, "color"), "name.color");
        const auto bar = data::AsObject(data::Member(json, "bar"), "bar");
        settings.barWidthPixels = Positive(bar, "width_pixels");
        settings.barHeightPixels = Positive(bar, "height_pixels");
        settings.gapPixels = Positive(bar, "gap_pixels");
        settings.backColor = Color(data::Member(bar, "back_color"), "bar.back_color");
        settings.backAlpha = Fraction(bar, "back_alpha", false);
        settings.fillColor = Color(data::Member(bar, "fill_color"), "bar.fill_color");
        settings.lowColor = Color(data::Member(bar, "low_color"), "bar.low_color");
        const auto engaged = data::AsObject(data::Member(json, "engaged"), "engaged");
        settings.lowHealth = Fraction(engaged, "low_health", true);
        settings.holdSeconds = Seconds(engaged, "hold_seconds");
        settings.fadeSeconds = Seconds(engaged, "fade_seconds");
        return settings;
    } catch (const std::exception& exception) {
        error = "overhead settings: " + std::string(exception.what());
        return std::nullopt;
    }
}

float HealthFraction(const std::uint32_t milliPoints, const std::uint32_t wholeMilliPoints) noexcept {
    const std::uint32_t whole = sim::DisplayPoints(wholeMilliPoints);
    if (whole == 0) return 0.0F;
    const std::uint32_t left = sim::DisplayPoints(milliPoints);
    return left >= whole ? 1.0F : static_cast<float>(left) / static_cast<float>(whole);
}

float OverheadBarAlpha(const OverheadPolicy policy, const std::optional<float> health, const std::optional<double> lastOwnHit,
                       const double nowSeconds, const OverheadSettings& settings) noexcept {
    if (!health) return 0.0F;
    if (policy == OverheadPolicy::Always || *health <= settings.lowHealth) return 1.0F;
    if (!lastOwnHit) return 0.0F;
    const double since = nowSeconds - *lastOwnHit;
    if (since <= settings.holdSeconds) return 1.0F;
    const double fading = since - settings.holdSeconds;
    if (fading >= settings.fadeSeconds) return 0.0F;
    return static_cast<float>(1.0 - fading / settings.fadeSeconds);
}

} // namespace object_fps_pvp::present
