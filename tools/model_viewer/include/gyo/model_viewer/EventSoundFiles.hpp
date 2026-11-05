#pragma once

#include "gyo/model_viewer/EventFiles.hpp"
#include "gyo/model_viewer/ModelViewerError.hpp"

#include "gyo/framework/EventSounds.hpp"

#include "engine/io/helpers/UserFiles.hpp"
#include "engine/model/AnimationEvents.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// gyo.event_sounds files (the framework's format, docs/framework/event-sounds.md)
// as the viewer opens, edits and saves them (Design Invariants 2 and 4, judgments
// S1-S3 of -event-sounds). Whether a file is valid is the framework's
// ParseEventSounds and BindEventSounds alone; the viewer writes the text and
// reads it back through them before anything is saved.
namespace Gyo::Tools::ModelViewer {

// The text of version 1: the fields in the format's order, events in name
// order, sync_seconds in the shortest form that reads back as the same double,
// gain in the shortest form that reads back as the same float.
[[nodiscard]] std::string WriteEventSounds(const Framework::EventSoundSet& set);

// Field-by-field equality: names and ids exactly, sync_seconds bit for bit,
// gain as the float the format holds, bit for bit.
[[nodiscard]] bool SameEventSounds(const Framework::EventSoundSet& left, const Framework::EventSoundSet& right) noexcept;

// WriteEventSounds, once the framework's ParseEventSounds accepts the text,
// BindEventSounds binds it to events and soundSeconds, and the parsed set is
// set. InvalidEventSounds with the framework's cause otherwise.
[[nodiscard]] Result<std::string, ModelViewerError> CheckedEventSoundsText(const Framework::EventSoundSet& set,
                                                                           const Engine::Model::AnimationEventSet& events,
                                                                           const Framework::SoundSeconds& soundSeconds);

struct OpenedEventSounds final {
    std::string path;
    Engine::IO::Helpers::FileStamp stamp; // what the file was when read
    Framework::EventSoundSet set;
};

// Reads path and has the framework parse it and bind it to events, the
// gyo.anim_events set with the id eventsAssetId, and to soundSeconds.
// OtherEvents when the file names another events asset (judgment S3: it is
// not shown, the viewer does not guess); InvalidEventSounds for what the
// framework refuses.
[[nodiscard]] Result<OpenedEventSounds, ModelViewerError> OpenEventSounds(const std::string& path,
                                                                          std::string_view eventsAssetId,
                                                                          const Engine::Model::AnimationEventSet& events,
                                                                          const Framework::SoundSeconds& soundSeconds);

// Replaces the file at path once CheckedEventSoundsText accepts set and the
// file is still what was opened (ChangedOnDisk otherwise; nothing is written).
// Returns the stamp of what was written.
[[nodiscard]] Result<Engine::IO::Helpers::FileStamp, ModelViewerError> SaveEventSounds(
    const std::string& path, const Engine::IO::Helpers::FileStamp& opened, const Framework::EventSoundSet& set,
    const Engine::Model::AnimationEventSet& events, const Framework::SoundSeconds& soundSeconds);

// Writes a new gyo.event_sounds file for the events file eventsAssetId and
// registers it (type "text", WriteNewText): OtherEvents when the set names
// another events file, InvalidEventSounds when CheckedEventSoundsText refuses
// it; nothing is written then.
[[nodiscard]] Result<NewEventsReport, ModelViewerError> CreateEventSounds(
    const NewEventsRequest& request, std::string_view eventsAssetId, const Framework::EventSoundSet& set,
    const Engine::Model::AnimationEventSet& events, const Framework::SoundSeconds& soundSeconds);

// When each sound of track's events starts, for a clip of clipSeconds played
// at speed (clip seconds per wall second): the framework's
// EventSoundOffsetSeconds(event seconds, clipSeconds, clipSeconds / speed,
// sync_seconds) only (Design Invariant 5), in seconds after the clip's start
// in wall time. Silent events and events the formula gives no time for are
// left out. In the order of the track's events.
struct ScheduledSound final {
    std::size_t event{};       // the index in the track
    std::string soundAssetId;
    float gain{};
    double offsetSeconds{};    // after the clip's start, may be negative
};
[[nodiscard]] std::vector<ScheduledSound> ScheduleSounds(const Framework::BoundEventSounds& sounds,
                                                         const Engine::Model::AnimationEventTrack& track,
                                                         double clipSeconds, double speed);

// Where a scheduled sound goes: the preview's audio output, or a test's record.
class SoundSink {
public:
    virtual ~SoundSink() = default;
    // sound starts at at (Engine time).
    virtual void Play(const ScheduledSound& sound, Engine::Time::TimePoint at) = 0;
    // Ends every sound sent so far, now.
    virtual void StopAll() = 0;
};

// Sends every sound of schedule for a clip that starts (in wall time) at
// clipStart: at clipStart + offsetSeconds, nothing else added. A sound whose
// start is before notBefore is left out (it would play late); returns how
// many were sent.
std::size_t PlayClipSounds(SoundSink& sink, const std::vector<ScheduledSound>& schedule,
                           Engine::Time::TimePoint clipStart, Engine::Time::TimePoint notBefore);

// The clip being played, as the window's clock has it this frame.
struct SoundClock final {
    bool playing{};
    bool loop{};
    double speed{};           // clip seconds per wall second
    double seconds{};         // clip seconds, in [0, durationSeconds]
    double durationSeconds{};
    bool wrapped{};           // the clock went past the end and round again this frame
};

// The sounds of a clip as it plays: each round of the clip sends its sounds
// once, at the times ScheduleSounds gives from the round's start. A looping
// clip has as many rounds ahead sent as the longest lead needs, so a sound
// that starts before its event is on time. A seek, a change of speed, of
// loop or of the schedule (a new version) stops what was sent and starts
// over; pausing stops; a clamped clip that reached its end lets its sounds
// ring out. A sound up to `lateness` late is still sent (the mixer's
// schedule delay covers it).
class SoundPlayback final {
public:
    void Update(SoundSink& sink, const std::vector<ScheduledSound>& schedule, std::uint64_t scheduleVersion,
                const SoundClock& clock, Engine::Time::TimePoint now, Engine::Time::Duration lateness);
    // Stops what was sent; the next Update starts over.
    void Stop(SoundSink& sink);

private:
    bool scheduled_{};
    std::uint64_t version_{};
    double speed_{};
    bool loop_{};
    Engine::Time::TimePoint roundStart_{};
    Engine::Time::TimePoint lastSent_{}; // the start of the last round sent
};

} // namespace Gyo::Tools::ModelViewer
