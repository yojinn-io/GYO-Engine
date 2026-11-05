#pragma once

#include "engine/model/AnimationEvents.hpp"
#include "gyo/framework/EventSounds.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace fps {

// A weapon's animation event sounds, read through the framework's
// gyo.event_sounds (docs/framework/event-sounds.md): the sounds as their text
// gave them and the gyo.anim_events set they bind to. The weapon presentation
// reads both; the sound output binds them once it has loaded the sounds and
// knows their lengths (v8-product-event-sounds-migrate). Client only (in
// app_support): the Match never links the framework.
struct WeaponEventSounds final {
    Engine::Model::AnimationEventSet events;
    Gyo::Tools::Framework::EventSoundSet sounds;
};

// Reads gyo.event_sounds text that must name the gyo.anim_events asset
// animationEventsAssetId (the framework knows no ids, so the caller checks).
// Nothing, with error, for a text the framework refuses or one written for
// another asset.
[[nodiscard]] std::optional<Gyo::Tools::Framework::EventSoundSet> ParseWeaponEventSounds(
    std::string_view text, std::string_view animationEventsAssetId, std::string& error);

// The framework's binding of a weapon's sounds to its event set and to the
// sounds' lengths. Nothing, with error, when it fails (as a whole).
[[nodiscard]] std::optional<Gyo::Tools::Framework::BoundEventSounds> BindWeaponEventSounds(
    const WeaponEventSounds& weapon, const Gyo::Tools::Framework::SoundSeconds& soundSeconds, std::string& error);

// When the sound of an animation event starts, in seconds from now, for an
// event at eventSeconds of a clip of clipSeconds played stretched over an
// action of actionSeconds that started anchorSeconds ago, and its gain. The
// moment is the framework's EventSoundOffsetSeconds (its Design Invariant 6);
// negative is late. Nothing when the framework gives no offset.
struct ScheduledEventSound final {
    double secondsFromNow{};
    float gain{};
};
[[nodiscard]] std::optional<ScheduledEventSound> ScheduleEventSound(const Gyo::Tools::Framework::EventSound& sound,
    double eventSeconds, double clipSeconds, double actionSeconds, double anchorSeconds) noexcept;

} // namespace fps
