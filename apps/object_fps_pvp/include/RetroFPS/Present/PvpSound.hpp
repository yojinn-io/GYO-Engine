#pragma once

#include "RetroFPS/Present/WeaponEventSounds.hpp"
#include "RetroFPS/Present/SoundCues.hpp"
#include "engine/base/Span.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fps::pvp {

// One clip's samples as the content holds them (a loaded sound asset).
struct SoundClipData final {
    Engine::Base::ConstSpan<std::int16_t> interleaved;
    std::uint16_t channels{};
    std::uint32_t sampleRate{};
};

// What acceptance reads of the sound output (diagnostics only; the product's
// own executable never turns them on): the output callbacks' anchors, every
// cue played with its time, and the mixer's counters.
struct SoundDiagnostics final {
    struct Anchor final {
        Engine::Time::TimePoint anchor{};
        std::uint64_t firstFrame{};
    };
    struct Cue final {
        std::optional<SoundCue> cue;      // nothing for an animation event's sound
        std::string event;                // that sound's event name
        Engine::Time::TimePoint at{};     // the time the cue belongs to
        Engine::Time::TimePoint played{}; // when Play handed it to the mixer
    };
    std::vector<Anchor> anchors;          // taken since the last call
    std::vector<Cue> cues;                // taken since the last call
    std::uint64_t anchorsDropped{};
    std::uint64_t submitted{}, onTime{}, late{}, queueFull{}, noVoice{};
    std::int64_t scheduleDelayNs{};
    std::string driver;
};

// The Client's sound output: the cues' clips in an Engine audio mixer played on
// the default device. Every cue is detected on the main thread, so one
// submitter carries them all. Without a device the Client stays silent and
// says so once in its log; a clip the mixer cannot take is a content error.
class PvpSound final {
public:
    using ClipSource = std::function<bool(const std::string& clipId, const std::function<void(const SoundClipData&)>& use,
                                          std::string& error)>;

    PvpSound();
    ~PvpSound();
    PvpSound(const PvpSound&) = delete;
    PvpSound& operator=(const PvpSound&) = delete;

    // Diagnostics for acceptance: call before Initialize; keeps up to this many
    // callback anchors between two TakeDiagnostics calls, and every cue played.
    void EnableDiagnostics(std::uint32_t anchorLogCapacity);
    [[nodiscard]] SoundDiagnostics TakeDiagnostics();
    // Loads every cue's clip through source and opens the output. False only
    // for a content error; a missing device leaves the sound silent.
    [[nodiscard]] bool Initialize(const SoundSettings& settings, const ClipSource& source, std::string& error);
    // Main thread. A cue at the Engine time it belongs to, with its side gains.
    void Play(SoundCue cue, Engine::Time::TimePoint at, StereoGains gains);
    // Main thread, after Initialize: loads the sounds of a weapon's animation
    // events through source and binds them to the weapon's events and to the
    // sounds' lengths (gyo.event_sounds), replacing an earlier binding. False
    // for a content error, or when the mixer has no clip slot left.
    [[nodiscard]] bool BindEventSounds(const fps::WeaponEventSounds& weapon, const ClipSource& source, std::string& error);
    // The bound sound of an animation event, or nullptr for a silent event.
    [[nodiscard]] const Gyo::Tools::Framework::EventSound* FindEventSound(std::string_view event) const noexcept;
    // Main thread. An event's sound at the Engine time it starts, centred at
    // the gain ScheduleEventSound gave.
    void PlayEventSound(const Gyo::Tools::Framework::EventSound& sound, Engine::Time::TimePoint at, float gain);
    [[nodiscard]] const SoundSettings& Settings() const noexcept { return settings_; }
    // One log line with the mixer's counters (log-contract: "PvP sound summary").
    void LogSummary() const;

private:
    struct Impl;
    SoundSettings settings_;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps::pvp
