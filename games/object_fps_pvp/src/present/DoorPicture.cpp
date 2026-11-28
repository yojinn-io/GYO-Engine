#include "object_fps_pvp/present/DoorPicture.hpp"

#include "data/JsonFields.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::present {

std::optional<DoorPictureSettings> ParseDoorPictureSettings(const std::string& text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the settings");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported version");
        DoorPictureSettings settings;
        settings.swingSeconds = static_cast<float>(data::Number(data::Member(json, "swing_seconds"), "swing_seconds"));
        if (!std::isfinite(settings.swingSeconds) || settings.swingSeconds < 0.0F || settings.swingSeconds > 10.0F) {
            throw std::runtime_error("swing_seconds is in [0, 10]");
        }
        return settings;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

float SwingToward(const float turned, const bool open, const double seconds, const DoorPictureSettings& settings) noexcept {
    const float target = open ? 1.0F : 0.0F;
    if (settings.swingSeconds <= 0.0F) return target;
    const auto step = static_cast<float>(std::max(0.0, seconds) / settings.swingSeconds);
    return turned < target ? std::min(target, turned + step) : std::max(target, turned - step);
}

} // namespace object_fps_pvp::present
