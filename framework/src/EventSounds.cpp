#include "gyo/framework/EventSounds.hpp"

#include "engine/audio/AudioFormat.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Gyo::Tools::Framework {
namespace {

EventSoundsError MakeError(const EventSoundsErrorCode code, std::string message, std::string path) {
    return EventSoundsError::Make(code, "gyo.event_sounds: " + std::move(message), std::move(path));
}

std::string EventPath(const std::string& event) {
    return "events.\"" + event + "\"";
}

bool InSomeClip(const Engine::Model::AnimationEventSet& events, const std::string& name) {
    for (const auto& track : events.tracks)
        for (const auto& event : track.events)
            if (event.name == name) return true;
    return false;
}

} // namespace

Result<void, EventSoundsError> CheckEventSoundSet(const EventSoundSet& sounds) {
    const auto invalid = [](std::string message, std::string path) {
        return Engine::Base::Err(MakeError(EventSoundsErrorCode::InvalidEventSounds, std::move(message), std::move(path)));
    };
    if (sounds.animationEventsAssetId.empty())
        return invalid("animation_events_asset_id is a nonempty string", "animation_events_asset_id");
    if (sounds.sounds.empty()) return invalid("events is a nonempty object", "events");
    for (std::size_t i = 0; i < sounds.sounds.size(); ++i) {
        const auto& sound = sounds.sounds[i];
        const auto path = EventPath(sound.event);
        if (sound.event.empty()) return invalid("an event name must not be empty", path);
        if (i > 0 && !(sounds.sounds[i - 1].event < sound.event))
            return invalid("one entry per event name, in ascending name order", path);
        if (sound.soundAssetId.empty())
            return invalid("sound_asset_id is a nonempty string", path + ".sound_asset_id");
        if (!std::isfinite(sound.syncSeconds) || sound.syncSeconds < 0)
            return invalid("sync_seconds is finite and not negative", path + ".sync_seconds");
        if (!Engine::Audio::IsValidGain(sound.gain))
            return invalid("gain is a GYO::Audio gain", path + ".gain");
    }
    return {};
}

const EventSound* BoundEventSounds::Find(const std::string_view event) const noexcept {
    const auto found = std::lower_bound(sounds.begin(), sounds.end(), event,
        [](const EventSound& sound, const std::string_view name) { return sound.event < name; });
    return found != sounds.end() && found->event == event ? &*found : nullptr;
}

Result<BoundEventSounds, EventSoundsError> BindEventSounds(const EventSoundSet& sounds,
                                                           const Engine::Model::AnimationEventSet& events,
                                                           const SoundSeconds& soundSeconds) {
    if (auto checked = CheckEventSoundSet(sounds); !checked) return Engine::Base::Err(std::move(checked).error());
    const auto unbound = [](std::string message, std::string path) {
        return Engine::Base::Err(MakeError(EventSoundsErrorCode::UnboundEventSounds, std::move(message), std::move(path)));
    };
    for (const auto& sound : sounds.sounds) {
        const auto path = EventPath(sound.event);
        if (!InSomeClip(events, sound.event))
            return unbound("no clip of the anim_events set has the event '" + sound.event + "'", path);
        const auto length = soundSeconds ? soundSeconds(sound.soundAssetId) : std::nullopt;
        if (!length || !std::isfinite(*length) || *length < 0)
            return unbound("no length for the sound '" + sound.soundAssetId + "'", path + ".sound_asset_id");
        if (!(sound.syncSeconds <= *length))
            return unbound("sync_seconds lies beyond the sound's " + std::to_string(*length) + " seconds",
                           path + ".sync_seconds");
    }
    return BoundEventSounds{sounds.sounds};
}

std::optional<double> EventSoundOffsetSeconds(const double eventSeconds, const double clipSeconds,
                                              const double actionSeconds, const double syncSeconds) noexcept {
    if (!std::isfinite(eventSeconds) || !std::isfinite(syncSeconds) || !std::isfinite(clipSeconds) ||
        !std::isfinite(actionSeconds) || clipSeconds <= 0 || actionSeconds <= 0)
        return std::nullopt;
    const double offset = eventSeconds * (actionSeconds / clipSeconds) - syncSeconds;
    if (!std::isfinite(offset)) return std::nullopt; // finite arguments can still overflow
    return offset;
}

} // namespace Gyo::Tools::Framework
