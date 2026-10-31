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
struct ClipSlot;
struct Command;
struct MixerState;
struct SubmitterSlot;
} // namespace Detail

struct MixerConfig final {
    std::uint32_t maxVoices = 32;      // playing plus scheduled voices, 1 to 1024
    std::uint32_t maxSubmitters = 4;   // 1 to 64, all allocated at creation
    std::uint32_t queueCapacity = 256; // per submitter, 1 to 65536: plays, stops and unloads
    // Clips each submitter may load while output runs, 0 to 256; 0 allocates
    // nothing. A slot is reused once its clip is unloaded and no voice reads it.
    std::uint32_t clipSlotsPerSubmitter = 0;
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

// One play sent through one submitter, never reused for the mixer's life: the
// submitter's index plus one in the top 7 bits, a sequence of that submitter
// (from 1, advanced only when a play is queued) below. Zero means the play was
// not queued because the queue was full.
struct SoundHandle final {
    std::uint64_t value = 0;
    [[nodiscard]] constexpr bool Valid() const noexcept { return value != 0; }
    constexpr explicit operator bool() const noexcept { return Valid(); }
    friend constexpr bool operator==(SoundHandle, SoundHandle) = default;
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
    // Stop requests (Stop and StopAll, one each) end in exactly one of matched
    // (it stopped at least one voice), unmatched (no voice of that sound was
    // still sounding or scheduled) and droppedQueueFull; stopsSubmitted is their
    // sum once none is in flight. stopsLate counts the matched requests that
    // stopped a started voice at a frame already rendered.
    std::uint64_t stopsSubmitted = 0;
    std::uint64_t stopsMatched = 0;
    std::uint64_t stopsUnmatched = 0;
    std::uint64_t stopsDroppedQueueFull = 0;
    std::uint64_t stopsLate = 0;
    // Unload requests are taken or droppedQueueFull; unloadsSubmitted is their
    // sum once none is in flight.
    std::uint64_t unloadsSubmitted = 0;
    std::uint64_t unloadsTaken = 0;
    std::uint64_t unloadsDroppedQueueFull = 0;
    Time::Duration maxLateness{};     // the latest late start
    std::uint64_t renderedFrames = 0;
    std::uint64_t clippedSamples = 0;
};

class Mixer;

// The producer end of one bounded single-producer, single-consumer queue. The
// first thread that calls Play, Stop, StopAll, LoadClip or UnloadClip owns it;
// calling them from another thread afterwards is a Programmer Error. It shares the mixer's state, so it may
// outlive the Mixer (its plays are then never rendered).
class Submitter final {
public:
    Submitter(Submitter&& other) noexcept;
    Submitter& operator=(Submitter&& other) noexcept;
    Submitter(const Submitter&) = delete;
    Submitter& operator=(const Submitter&) = delete;
    ~Submitter();

    // An invalid handle when the queue is full; the request is then dropped and
    // counted. A clip neither in the mixer's bank nor loaded through this
    // submitter (and not unloaded since), a gain outside [0, kMaxGain],
    // another thread than the owner, and a moved-from Submitter are Programmer
    // Errors (as for every member below).
    SoundHandle Play(const PlayRequest& request);
    // Ends the sound at the frame `at` maps to, by the same rule as a play's
    // start (delay included), from the anchor of the block that takes the stop.
    // A sound stopped before it started, at or before its start, stays silent; a
    // started sound stopped at a frame already rendered stops at the first frame
    // of that block, counted late. With several stops, the earliest frame wins.
    // An invalid handle does nothing else and returns true (the call still
    // claims the submitter for its thread). Another submitter's handle, or one this submitter never returned, is
    // a Programmer Error. False when the queue is full (counted; the sound keeps
    // playing).
    [[nodiscard]] bool Stop(SoundHandle sound, Time::TimePoint at);
    // The same for every sound this submitter sent before the call; one stop
    // request in the counters.
    [[nodiscard]] bool StopAll(Time::TimePoint at);

    // Copies a clip into one of this submitter's slots, on this thread, also
    // while output runs, and returns its id for Play. The slots of clips whose
    // unload the audio thread has taken and no voice reads any more are freed
    // first. InvalidClip as ClipBank::Add; NoClipSlot when every slot is loaded
    // or not handed back yet (an output that never renders hands none back).
    [[nodiscard]] Base::Result<ClipId, AudioError> LoadClip(Base::ConstSpan<std::int16_t> interleaved,
                                                            std::uint16_t channels, std::uint32_t sampleRate);
    // Releases a loaded clip behind every play already queued: sounds already
    // sent play to their end (stop them first to free it sooner). Playing or
    // unloading it afterwards, a clip of another submitter and a bank clip are
    // Programmer Errors. False when the queue is full (counted; the clip stays
    // loaded and may be unloaded again).
    [[nodiscard]] bool UnloadClip(ClipId clip);

private:
    friend class Mixer;
    Submitter(std::shared_ptr<Detail::MixerState> state, Detail::SubmitterSlot* slot) noexcept;
    void ClaimOwner();
    bool Enqueue(const Detail::Command& command);
    bool SendStop(const Detail::Command& command);
    // This submitter's slot of a runtime clip that is loaded under that id, or null.
    [[nodiscard]] Detail::ClipSlot* LoadedClip(ClipId clip) const noexcept;

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
    // the start of this block. Every queued request is taken in its queue's
    // order (a stop after the plays it names) and a play starts at
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
