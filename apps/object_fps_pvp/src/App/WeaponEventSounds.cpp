#include "RetroFPS/App/WeaponEventSounds.hpp"

#include "gyo/framework/json/EventSoundsText.hpp"

namespace fps {

std::optional<Gyo::Tools::Framework::EventSoundSet> ParseWeaponEventSounds(
    const std::string_view text, const std::string_view animationEventsAssetId, std::string& error) {
    auto parsed = Gyo::Tools::Framework::ParseEventSounds(text);
    if (!parsed) {
        error = "event sounds: " + Engine::Base::Describe(parsed.error());
        return std::nullopt;
    }
    if (parsed.value().animationEventsAssetId != animationEventsAssetId) {
        error = "event sounds name animation events '" + parsed.value().animationEventsAssetId + "', not '" +
                std::string(animationEventsAssetId) + "'";
        return std::nullopt;
    }
    return std::move(parsed).value();
}

std::optional<Gyo::Tools::Framework::BoundEventSounds> BindWeaponEventSounds(
    const WeaponEventSounds& weapon, const Gyo::Tools::Framework::SoundSeconds& soundSeconds, std::string& error) {
    auto bound = Gyo::Tools::Framework::BindEventSounds(weapon.sounds, weapon.events, soundSeconds);
    if (!bound) {
        error = "event sounds: " + Engine::Base::Describe(bound.error());
        return std::nullopt;
    }
    return std::move(bound).value();
}

std::optional<ScheduledEventSound> ScheduleEventSound(const Gyo::Tools::Framework::EventSound& sound,
    const double eventSeconds, const double clipSeconds, const double actionSeconds,
    const double anchorSeconds) noexcept {
    const auto offset = Gyo::Tools::Framework::EventSoundOffsetSeconds(eventSeconds, clipSeconds, actionSeconds,
                                                                       sound.syncSeconds);
    if (!offset) return std::nullopt;
    return ScheduledEventSound{*offset - anchorSeconds, sound.gain};
}

} // namespace fps
