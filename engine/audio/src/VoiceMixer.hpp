#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "engine/audio/AudioError.hpp"
#include "engine/audio/ClipBank.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"

namespace Engine::Audio::Detail {

// One play whose start is already a sample position of the output.
struct ScheduledPlay final {
    ClipId clip;
    std::uint64_t startFrame = 0;
    float gainLeft = 1.0f;
    float gainRight = 1.0f;
};

enum class StartOutcome : std::uint8_t { OnTime, Late, NoVoice };

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
// the output is split into blocks. Schedule and Render run on one thread (the
// audio thread); Stats and SetMasterGain may run on any thread.
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

    // A clip outside the bank and a gain outside [0, kMaxGain] are Programmer
    // Errors. A start before the next frame to render is late: the voice starts
    // at that frame instead. Allocates nothing.
    StartOutcome Schedule(const ScheduledPlay& play);

    // Fills the interleaved stereo block and advances renderedFrames by its frame
    // count. An odd sample count is a Programmer Error checked by the caller.
    // Allocates, locks and waits for nothing.
    void Render(Base::Span<std::int16_t> interleavedOut) noexcept;

    // Any thread. A gain outside [0, kMaxGain] is a Programmer Error.
    void SetMasterGain(float gain);
    [[nodiscard]] VoiceMixerStats Stats() const noexcept;
    [[nodiscard]] std::uint64_t RenderedFrames() const noexcept;
    [[nodiscard]] const ClipBank& Clips() const noexcept { return clips_; }

private:
    struct Voice final {
        bool active = false;
        std::uint64_t startFrame = 0;
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

    ClipBank clips_;
    std::vector<Voice> voices_;
    std::vector<std::int64_t> accumulator_; // kChunkFrames * kChannels, allocated once
    std::uint64_t nextFrame_ = 0;           // audio thread's own copy of renderedFrames
    std::atomic<std::int64_t> masterGainQ16_{1 << 16};
    Counters counters_;
};

} // namespace Engine::Audio::Detail
