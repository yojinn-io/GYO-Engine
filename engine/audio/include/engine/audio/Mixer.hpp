#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include "engine/audio/AudioError.hpp"
#include "engine/audio/ClipBank.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"
#include "engine/time/MonotonicClock.hpp"

namespace Engine::Audio {

namespace Detail {
struct MixerState;
struct SubmitterSlot;
} // namespace Detail

struct MixerConfig final {
    std::uint32_t maxVoices = 32;      // playing plus scheduled voices, 1 to 1024
    std::uint32_t maxSubmitters = 4;   // 1 to 64, all allocated at creation
    std::uint32_t queueCapacity = 256; // per submitter, 1 to 65536
    // Added to every mapped start, so a request sent a little after its time still
    // starts on time; it is also the least latency of a sound played "now". 0 to
    // 1 s. The default covers a consumer at 30 FPS (a request up to 33 ms late)
    // with SDL's default device block on macOS (callbacks up to 21.5 ms apart);
    // a consumer with a higher frame rate or smaller blocks may lower it
    // (v8-engine-audio-delay-default).
    Time::Duration scheduleDelay = std::chrono::duration_cast<Time::Duration>(std::chrono::milliseconds(60));
};

// One sound, played once. The consumer turns direction and distance into gains.
struct PlayRequest final {
    ClipId clip;
    Time::TimePoint at{};    // the Engine time the sound belongs to
    float gainLeft = 1.0f;   // 0 to kMaxGain
    float gainRight = 1.0f;
};

// Counters for display and acceptance; never for decisions. Each request ends
// in exactly one of playedOnTime, playedLate, droppedQueueFull, droppedNoVoice,
// decided when the audio thread takes it (a play counted on time may still be
// ahead). The sum equals submitted once no request is in flight: the audio
// thread has taken every queued request and no producer is playing. Counters are
// read one by one, so a snapshot taken meanwhile may differ either way; do not
// subtract them to count requests in flight.
struct MixerStats final {
    std::uint64_t submitted = 0;
    std::uint64_t playedOnTime = 0;
    std::uint64_t playedLate = 0;
    std::uint64_t droppedQueueFull = 0;
    std::uint64_t droppedNoVoice = 0;
    Time::Duration maxLateness{};     // the latest late start
    std::uint64_t renderedFrames = 0;
    std::uint64_t clippedSamples = 0;
};

class Mixer;

// The producer end of one bounded single-producer, single-consumer queue. The
// first thread that plays through it owns it; playing from another thread
// afterwards is a Programmer Error. It shares the mixer's state, so it may
// outlive the Mixer (its plays are then never rendered).
class Submitter final {
public:
    Submitter(Submitter&& other) noexcept;
    Submitter& operator=(Submitter&& other) noexcept;
    Submitter(const Submitter&) = delete;
    Submitter& operator=(const Submitter&) = delete;
    ~Submitter();

    // False when the queue is full; the request is then dropped and counted. A
    // clip outside the mixer's bank, a gain outside [0, kMaxGain], another
    // thread than the owner, and a moved-from Submitter are Programmer Errors.
    bool Play(const PlayRequest& request);

private:
    friend class Mixer;
    Submitter(std::shared_ptr<Detail::MixerState> state, Detail::SubmitterSlot* slot) noexcept;

    std::shared_ptr<Detail::MixerState> state_; // keeps the mixer's state alive
    Detail::SubmitterSlot* slot_ = nullptr;     // this submitter's queue inside it
};

// Mixes one-shot plays scheduled by Engine timestamps into 48 kHz stereo PCM16.
// Render runs on one thread, the audio thread of an output backend; the other
// members may run on any thread.
class Mixer final {
public:
    // InvalidConfig for a value outside MixerConfig's documented limits.
    [[nodiscard]] static Base::Result<Mixer, AudioError> Create(const MixerConfig& config, ClipBank&& clips);

    // A moved-from Mixer only renders silence; its other members are Programmer Errors.
    Mixer(Mixer&&) noexcept;
    Mixer& operator=(Mixer&&) noexcept;
    Mixer(const Mixer&) = delete;
    Mixer& operator=(const Mixer&) = delete;
    ~Mixer();

    // Any thread, at any time. TooManySubmitters once maxSubmitters have been
    // created over the mixer's life: a destroyed Submitter does not give its
    // queue back, so create one per producer thread and keep it.
    [[nodiscard]] Base::Result<Submitter, AudioError> CreateSubmitter();
    // Any thread. A gain outside [0, kMaxGain] is a Programmer Error.
    void SetMasterGain(float gain);
    [[nodiscard]] MixerStats Stats() const noexcept;

    // The audio thread, once per block. anchor is the Engine time read once at
    // the start of this block. Every queued request is taken and starts at
    //   renderedFrames + floor((at - anchor) * 48000 + 1/2) + delayFrames,
    // with at - anchor clamped to one hour either way and delayFrames the
    // scheduleDelay rounded the same way; or, when that is before this block,
    // at the block's first frame (late).
    // An odd sample count is a Programmer Error of the backend; Render writes
    // silence to the last sample then. On a moved-from Mixer it writes silence.
    // Allocates, locks and waits for nothing.
    void Render(Base::Span<std::int16_t> interleavedOut, Time::TimePoint anchor) noexcept;

private:
    explicit Mixer(std::shared_ptr<Detail::MixerState> state) noexcept;
    std::shared_ptr<Detail::MixerState> state_;
};

} // namespace Engine::Audio
