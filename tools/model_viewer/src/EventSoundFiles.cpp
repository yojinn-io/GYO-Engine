#include "gyo/model_viewer/EventSoundFiles.hpp"

#include "FileAccess.hpp"
#include "JsonText.hpp"

#include "gyo/framework/json/EventSoundsText.hpp"

#include "engine/base/Assert.hpp"
#include "engine/base/Fnv1a.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>

namespace Gyo::Tools::ModelViewer {
namespace {

// The shortest decimal that reads back as the same float: the format holds a
// gain as a float (docs/framework/event-sounds.md, rule 6).
void AppendGain(std::string& out, const float gain) {
    if (gain == 0.0F && std::signbit(gain)) {
        out += "-0.0";
        return;
    }
    std::array<char, 32> digits{};
    const auto written = std::to_chars(digits.data(), digits.data() + digits.size(), gain);
    GYO_ASSERT(written.ec == std::errc{});
    out.append(digits.data(), written.ptr);
}

ModelViewerError Refused(const Framework::EventSoundsError& error, const std::string& where) {
    return ModelViewerError::Make(ModelViewerErrorCode::InvalidEventSounds, "the framework refuses the event sounds",
                                  Engine::Base::CauseDetail(error, where));
}

} // namespace

std::string WriteEventSounds(const Framework::EventSoundSet& set) {
    std::string out = "{\n  \"schema\": \"gyo.event_sounds\",\n  \"version\": 1,\n  \"animation_events_asset_id\": ";
    AppendString(out, set.animationEventsAssetId);
    out += ",\n  \"events\": {";
    for (std::size_t index = 0; index < set.sounds.size(); ++index) {
        const auto& sound = set.sounds[index];
        out += index == 0 ? "\n    " : ",\n    ";
        AppendString(out, sound.event);
        out += ": { \"sound_asset_id\": ";
        AppendString(out, sound.soundAssetId);
        out += ", \"sync_seconds\": ";
        AppendSeconds(out, sound.syncSeconds);
        out += ", \"gain\": ";
        AppendGain(out, sound.gain);
        out += " }";
    }
    out += set.sounds.empty() ? "}\n}\n" : "\n  }\n}\n";
    return out;
}

bool SameEventSounds(const Framework::EventSoundSet& left, const Framework::EventSoundSet& right) noexcept {
    if (left.animationEventsAssetId != right.animationEventsAssetId || left.sounds.size() != right.sounds.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.sounds.size(); ++index) {
        const auto& a = left.sounds[index];
        const auto& b = right.sounds[index];
        if (a.event != b.event || a.soundAssetId != b.soundAssetId ||
            std::bit_cast<std::uint64_t>(a.syncSeconds) != std::bit_cast<std::uint64_t>(b.syncSeconds) ||
            std::bit_cast<std::uint32_t>(a.gain) != std::bit_cast<std::uint32_t>(b.gain)) {
            return false;
        }
    }
    return true;
}

Result<std::string, ModelViewerError> CheckedEventSoundsText(const Framework::EventSoundSet& set,
                                                             const Engine::Model::AnimationEventSet& events,
                                                             const Framework::SoundSeconds& soundSeconds) {
    std::string text = WriteEventSounds(set);
    auto parsed = Framework::ParseEventSounds(text);
    if (!parsed) return Engine::Base::Err(Refused(parsed.error(), "the text to write"));
    if (auto bound = Framework::BindEventSounds(parsed.value(), events, soundSeconds); !bound) {
        return Engine::Base::Err(Refused(bound.error(), "the text to write"));
    }
    if (!SameEventSounds(parsed.value(), set)) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidEventSounds,
                                                        "the written text reads back as other event sounds",
                                                        "events not in name order, or a value without a form in the text"));
    }
    return text;
}

Result<OpenedEventSounds, ModelViewerError> OpenEventSounds(const std::string& path, const std::string_view eventsAssetId,
                                                            const Engine::Model::AnimationEventSet& events,
                                                            const Framework::SoundSeconds& soundSeconds) {
    auto stamp = Probe(path);
    if (!stamp) return Engine::Base::Err(std::move(stamp).error());
    auto bytes = ReadAll(path, kMaxTextBytes);
    if (!bytes) return Engine::Base::Err(std::move(bytes).error());
    const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    if (Engine::Base::Fnv1a64(text) != stamp->contentHash) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::ChangedOnDisk,
                                                        "the file changed while it was read", path));
    }
    auto parsed = Framework::ParseEventSounds(text);
    if (!parsed) return Engine::Base::Err(Refused(parsed.error(), path));
    // The framework leaves which anim_events set a file names to the caller.
    if (parsed->animationEventsAssetId != eventsAssetId) {
        return Engine::Base::Err(ModelViewerError::Make(
            ModelViewerErrorCode::OtherEvents, "the event sounds name another gyo.anim_events file than the one opened",
            path + "; animation_events_asset_id " + parsed->animationEventsAssetId + ", opened " + std::string(eventsAssetId)));
    }
    if (auto bound = Framework::BindEventSounds(parsed.value(), events, soundSeconds); !bound) {
        return Engine::Base::Err(Refused(bound.error(), path));
    }
    return OpenedEventSounds{path, stamp.value(), std::move(parsed).value()};
}

Result<Engine::IO::Helpers::FileStamp, ModelViewerError> SaveEventSounds(
    const std::string& path, const Engine::IO::Helpers::FileStamp& opened, const Framework::EventSoundSet& set,
    const Engine::Model::AnimationEventSet& events, const Framework::SoundSeconds& soundSeconds) {
    auto text = CheckedEventSoundsText(set, events, soundSeconds);
    if (!text) return Engine::Base::Err(std::move(text).error());
    auto current = Probe(path);
    if (!current) return Engine::Base::Err(std::move(current).error());
    if (current.value() != opened) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::ChangedOnDisk,
                                                        "the file changed since it was opened", path));
    }
    if (auto written = WriteReplacing(path, std::as_bytes(std::span(text->data(), text->size()))); !written) {
        return Engine::Base::Err(std::move(written).error());
    }
    return StampOfWritten(path, *text);
}

Result<NewEventsReport, ModelViewerError> CreateEventSounds(const NewEventsRequest& request,
                                                            const std::string_view eventsAssetId,
                                                            const Framework::EventSoundSet& set,
                                                            const Engine::Model::AnimationEventSet& events,
                                                            const Framework::SoundSeconds& soundSeconds) {
    if (set.animationEventsAssetId != eventsAssetId) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::OtherEvents,
                                                        "the event sounds name another gyo.anim_events file than the one opened",
                                                        set.animationEventsAssetId));
    }
    auto text = CheckedEventSoundsText(set, events, soundSeconds);
    if (!text) return Engine::Base::Err(std::move(text).error());
    return WriteNewText(request, *text);
}

std::vector<ScheduledSound> ScheduleSounds(const Framework::BoundEventSounds& sounds,
                                           const Engine::Model::AnimationEventTrack& track, const double clipSeconds,
                                           const double speed) {
    std::vector<ScheduledSound> schedule;
    if (!(speed > 0.0)) return schedule;
    const double actionSeconds = clipSeconds / speed;
    for (std::size_t index = 0; index < track.events.size(); ++index) {
        const auto& event = track.events[index];
        const Framework::EventSound* sound = sounds.Find(event.name);
        if (sound == nullptr) continue;
        const auto offset = Framework::EventSoundOffsetSeconds(event.seconds, clipSeconds, actionSeconds, sound->syncSeconds);
        if (!offset) continue;
        schedule.push_back({index, sound->soundAssetId, sound->gain, *offset});
    }
    return schedule;
}

std::size_t PlayClipSounds(SoundSink& sink, const std::vector<ScheduledSound>& schedule,
                           const Engine::Time::TimePoint clipStart, const Engine::Time::TimePoint notBefore) {
    std::size_t sent = 0;
    for (const auto& sound : schedule) {
        const auto at = clipStart + std::chrono::duration_cast<Engine::Time::Duration>(
                                        std::chrono::duration<double>(sound.offsetSeconds));
        if (at < notBefore) continue;
        sink.Play(sound, at);
        ++sent;
    }
    return sent;
}

namespace {

Engine::Time::Duration Wall(const double seconds) {
    return std::chrono::duration_cast<Engine::Time::Duration>(std::chrono::duration<double>(seconds));
}

// How many rounds of a looping clip are sent ahead of the one playing: one,
// or more when a sound leads its event by more than a round.
int LeadRounds(const std::vector<ScheduledSound>& schedule, const double round) {
    double lead = 0.0;
    for (const auto& sound : schedule) lead = std::max(lead, -sound.offsetSeconds);
    return 1 + static_cast<int>(std::ceil(lead / round));
}

} // namespace

void SoundPlayback::Stop(SoundSink& sink) {
    if (scheduled_) sink.StopAll();
    scheduled_ = false;
}

void SoundPlayback::Update(SoundSink& sink, const std::vector<ScheduledSound>& schedule, const std::uint64_t scheduleVersion,
                           const SoundClock& clock, const Engine::Time::TimePoint now,
                           const Engine::Time::Duration lateness) {
    if (!clock.playing || !(clock.speed > 0.0) || !(clock.durationSeconds > 0.0) || schedule.empty()) {
        // A clamped clip at its end rings out; anything else stops.
        if (!(clock.seconds >= clock.durationSeconds && !clock.loop)) Stop(sink);
        scheduled_ = false;
        return;
    }
    const double round = clock.durationSeconds / clock.speed;
    const auto roundLength = Wall(round);
    const auto clipStart = now - Wall(clock.seconds / clock.speed);
    const auto notBefore = now - lateness;
    if (scheduled_ && clock.wrapped && clock.loop) roundStart_ += roundLength;
    const auto tolerance = std::chrono::milliseconds(50);
    const bool onCourse = scheduled_ && version_ == scheduleVersion && speed_ == clock.speed && loop_ == clock.loop &&
                          roundStart_ - clipStart < tolerance && clipStart - roundStart_ < tolerance;
    if (onCourse) {
        // Keep the rounds ahead: each wrap adds the next one.
        while (clock.loop && lastSent_ < roundStart_ + LeadRounds(schedule, round) * roundLength) {
            lastSent_ += roundLength;
            static_cast<void>(PlayClipSounds(sink, schedule, lastSent_, notBefore));
        }
        return;
    }
    Stop(sink);
    roundStart_ = clipStart;
    lastSent_ = clipStart;
    static_cast<void>(PlayClipSounds(sink, schedule, clipStart, notBefore));
    if (clock.loop) {
        for (int ahead = LeadRounds(schedule, round); ahead > 0; --ahead) {
            lastSent_ += roundLength;
            static_cast<void>(PlayClipSounds(sink, schedule, lastSent_, notBefore));
        }
    }
    scheduled_ = true;
    version_ = scheduleVersion;
    speed_ = clock.speed;
    loop_ = clock.loop;
}

} // namespace Gyo::Tools::ModelViewer
