#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <vector>

#include "engine/audio/AudioError.hpp"
#include "engine/audio/ClipBank.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"

namespace Engine::Audio::Detail {

// One play whose start is already a sample position of the output. Signed, so
// a time before the first rendered frame is a position too (and is late).
struct ScheduledPlay final {
    ClipId clip;
    std::int64_t startFrame = 0;
    float gainLeft = 1.0f;
    float gainRight = 1.0f;
    std::uint64_t cookie = 0; // opaque to the voice mixer; Stop matches it
    // A clip outside the bank: its samples, which stay valid while the voice
    // plays, and a nonzero lease mark reported in Ended() when the voice ends.
    const ClipView* view = nullptr;
    std::uint64_t lease = 0;
};

enum class StartOutcome : std::uint8_t { OnTime, Late, NoVoice };

// What one Stop call did: how many voices it stopped, and whether any of them
// had started and was stopped at a frame already rendered.
struct StopResult final {
    std::uint32_t stopped = 0;
    bool late = false;
};

struct VoiceMixerStats final {
    std::uint64_t accepted = 0;      // Schedule calls; onTime + late + noVoice
    std::uint64_t onTime = 0;
    std::uint64_t late = 0;          // started at the next frame to render instead
    std::uint64_t noVoice = 0;       // dropped: every voice was busy
    std::uint64_t maxLateFrames = 0;
    std::uint64_t renderedFrames = 0;
    std::uint64_t clippedSamples = 0; // output samples saturated to int16
};

// The sample-domain core of the mixer: a fixed set of voices, each playing one
// clip from a start frame, mixed in integer arithmetic so the output depends
// only on the clips, the scheduled plays and the frames rendered, never on how
// the output is split into blocks. Schedule, Stop and Render run on one thread
// (the audio thread); Stats and SetMasterGain may run on any thread.
//
// The renderedFrames counter is the authority for where playback is; nothing
// outside the mixer keeps a copy of it to decide anything.
class VoiceMixer final {
public:
    // Fails with InvalidConfig when maxVoices is zero or above 1024.
    [[nodiscard]] static Base::Result<VoiceMixer, AudioError> Create(std::uint32_t maxVoices, ClipBank&& clips);

    VoiceMixer(VoiceMixer&& other) noexcept;
    VoiceMixer& operator=(VoiceMixer&&) = delete;
    VoiceMixer(const VoiceMixer&) = delete;
    VoiceMixer& operator=(const VoiceMixer&) = delete;
    ~VoiceMixer() = default;

    // A clip outside the bank (without a view) and a gain outside [0, kMaxGain]
    // are Programmer Errors. A start before the next frame to render is late: the voice starts
    // at that frame instead. Allocates nothing.
    StartOutcome Schedule(const ScheduledPlay& play);
    // The same without the checks, for the audio thread: the caller has already
    // checked the clip and the gains (Mixer's Submitter does, on its producer).
    StartOutcome ScheduleChecked(const ScheduledPlay& play) noexcept;

    // The audio thread. Stops every sounding or scheduled voice whose cookie,
    // masked, equals cookie: a voice that has not started is freed at once when
    // stopFrame is at or before its start, else it ends at stopFrame; a voice
    // that has started ends at stopFrame, or is freed at once when stopFrame is
    // at or before the next frame to render (late when before it). The earliest
    // stop frame of a voice wins.
    StopResult Stop(std::uint64_t cookie, std::uint64_t mask, std::int64_t stopFrame) noexcept;

    // The audio thread. The lease marks of the voices that ended (reached their
    // end or stop frame, or were freed by Stop) since ClearEnded, at most
    // maxVoices of them between two Stop or Render calls: the caller drains them
    // after each.
    [[nodiscard]] Base::ConstSpan<std::uint64_t> Ended() const noexcept {
        return Base::ConstSpan<std::uint64_t>(ended_.data(), endedCount_);
    }
    void ClearEnded() noexcept { endedCount_ = 0; }

    // Fills the interleaved stereo block and advances renderedFrames by its frame
    // count. An odd sample count is the caller's error; it cannot be asserted
    // here (noexcept), so the stray last sample is written as silence.
    // Allocates, locks and waits for nothing.
    void Render(Base::Span<std::int16_t> interleavedOut) noexcept;

    // Any thread. A gain outside [0, kMaxGain] is a Programmer Error.
    void SetMasterGain(float gain);
    [[nodiscard]] VoiceMixerStats Stats() const noexcept;
    [[nodiscard]] std::uint64_t RenderedFrames() const noexcept;
    // The audio thread's next frame to render (the same count, without an atomic).
    [[nodiscard]] std::int64_t NextFrame() const noexcept { return nextFrame_; }
    [[nodiscard]] const ClipBank& Clips() const noexcept { return clips_; }

private:
    struct Voice final {
        bool active = false;
        std::int64_t startFrame = 0;
        std::int64_t stopFrame = std::numeric_limits<std::int64_t>::max(); // the first frame not mixed
        std::uint64_t cookie = 0;
        std::uint64_t lease = 0;
        std::uint32_t position = 0; // frames of the clip already mixed
        std::int64_t gainLeftQ16 = 0;
        std::int64_t gainRightQ16 = 0;
        ClipView clip;
    };
    struct Counters final {
        std::atomic<std::uint64_t> accepted{0}, onTime{0}, late{0}, noVoice{0}, maxLateFrames{0};
        std::atomic<std::uint64_t> renderedFrames{0}, clippedSamples{0};
    };

    VoiceMixer(std::uint32_t maxVoices, ClipBank&& clips);
    void RenderChunk(std::int16_t* out, std::uint32_t frames) noexcept;
    void End(Voice& voice) noexcept;

    ClipBank clips_;
    std::vector<ClipView> views_; // one per clip, indexed by ClipId::value - 1
    std::vector<Voice> voices_;
    std::vector<std::int64_t> accumulator_; // kChunkFrames * kChannels, allocated once
    std::vector<std::uint64_t> ended_;      // maxVoices lease marks, allocated once
    std::size_t endedCount_ = 0;
    std::int64_t nextFrame_ = 0;            // audio thread's own copy of renderedFrames
    std::atomic<std::int64_t> masterGainQ16_{1 << 16};
    Counters counters_;
};

} // namespace Engine::Audio::Detail
