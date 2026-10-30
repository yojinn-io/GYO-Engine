#pragma once

#include "RetroFPS/Pvp/SoundCues.hpp"
#include "engine/base/Span.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace fps::pvp {

// One clip's samples as the content holds them (a loaded sound asset).
struct SoundClipData final {
    Engine::Base::ConstSpan<std::int16_t> interleaved;
    std::uint16_t channels{};
    std::uint32_t sampleRate{};
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

    // Loads every cue's clip through source and opens the output. False only
    // for a content error; a missing device leaves the sound silent.
    [[nodiscard]] bool Initialize(const SoundSettings& settings, const ClipSource& source, std::string& error);
    // Main thread. A cue at the Engine time it belongs to, with its side gains.
    void Play(SoundCue cue, Engine::Time::TimePoint at, StereoGains gains);
    [[nodiscard]] const SoundSettings& Settings() const noexcept { return settings_; }
    // The length of a cue's clip in seconds (0 before Initialize).
    [[nodiscard]] double ClipSeconds(SoundCue cue) const noexcept;
    // One log line with the mixer's counters (log-contract: "PvP sound summary").
    void LogSummary() const;

private:
    struct Impl;
    SoundSettings settings_;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps::pvp
