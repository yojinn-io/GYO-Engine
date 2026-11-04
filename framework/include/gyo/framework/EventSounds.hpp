#pragma once

#include "engine/model/AnimationEvents.hpp"
#include "gyo/framework/FrameworkError.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// gyo.event_sounds version 1, as structure (docs/framework/event-sounds.md):
// which sound plays at an animation event, and which point of the sound lands
// on the event's moment. A composition of Engine assets: event names are the
// opaque names of a gyo.anim_events set, sounds are asset ids, seconds and
// gains are Engine values. The JSON binding (GYO::ToolFrameworkJson) reads the
// text; nothing here opens or writes a file.

namespace Gyo::Tools::Framework {

inline constexpr std::string_view EventSoundsSchema = "gyo.event_sounds";
inline constexpr int EventSoundsVersion = 1;

struct EventSound final {
    std::string event;        // an event name of the bound gyo.anim_events set
    std::string soundAssetId; // an asset id of type sound
    double syncSeconds{};     // the point of the sound, in its own seconds, that lands on the event's moment
    float gain{};             // a GYO::Audio gain
};

struct EventSoundSet final {
    std::string animationEventsAssetId;
    std::vector<EventSound> sounds; // one per event name, in ascending name order
};

// Every rule of the format that needs no other asset: a nonempty
// animationEventsAssetId; one or more sounds, with nonempty names in strictly
// ascending order; a nonempty soundAssetId; a finite, non-negative
// syncSeconds; a gain Engine::Audio::IsValidGain accepts. InvalidEventSounds
// with the path of the offending value otherwise.
[[nodiscard]] Result<void, EventSoundsError> CheckEventSoundSet(const EventSoundSet& sounds);

// The length in seconds of the sound with an asset id, as the caller loaded it
// through Engine, or nothing for a sound it does not know.
using SoundSeconds = std::function<std::optional<double>(std::string_view soundAssetId)>;

struct BoundEventSounds final {
    std::vector<EventSound> sounds; // in ascending name order, as BindEventSounds returns them

    // The sound of an event, or nullptr for an event that is silent. Relies on
    // the order of sounds: a value built by hand out of order misses names.
    [[nodiscard]] const EventSound* Find(std::string_view event) const noexcept;
};

// Binds a set to the gyo.anim_events set it names and to its sounds' lengths:
// every name is the name of an event in some clip of events, and every
// syncSeconds is at most its sound's length, which soundSeconds answers finite
// and not negative. A set CheckEventSoundSet refuses gives its error;
// otherwise UnboundEventSounds. Binding fails as a whole.
// events is a set Engine's ParseAnimationEvents returned (so its names have
// Engine's form, which this binding relies on instead of copying the rule),
// and the caller checks that it is the one animationEventsAssetId names.
// soundSeconds is called once per entry, in name order, until one fails; an
// exception it throws is not caught.
[[nodiscard]] Result<BoundEventSounds, EventSoundsError> BindEventSounds(const EventSoundSet& sounds,
                                                                         const Engine::Model::AnimationEventSet& events,
                                                                         const SoundSeconds& soundSeconds);

// When an event's sound starts, in seconds after the action's start, for an
// animation clip of clipSeconds played stretched over the action's
// actionSeconds: the event's moment in the action, less the sound's sync
// point. May be negative (the sound leads the action). Nothing for a clip or
// action without a positive finite length, a non-finite moment or sync point,
// or a result that is not finite (it overflowed). The one schedule of event
// sounds (Design Invariant 6).
[[nodiscard]] std::optional<double> EventSoundOffsetSeconds(double eventSeconds, double clipSeconds,
                                                            double actionSeconds, double syncSeconds) noexcept;

} // namespace Gyo::Tools::Framework
