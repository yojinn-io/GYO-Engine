#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/audio/AudioError.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/base/Result.hpp"
#include "engine/time/MonotonicClock.hpp"

namespace Engine::Audio::Sdl {

struct SdlAudioOutputConfig final {
    // Frames per device block; 0 keeps SDL's choice. A hint to SDL: the device
    // may still choose another size (DeviceSampleFrames reports it).
    std::uint32_t deviceSampleFrames = 0;
    // How many callback anchors the output keeps for TakeAnchors (0: none, the
    // default). For logs and acceptance only; a full log drops new anchors and
    // counts them (SdlAudioOutputStats::anchorsDropped).
    std::uint32_t anchorLogCapacity = 0;
};

// One callback's anchor: the Engine time it read and the mixer's first frame of
// the block it rendered. With these, a request's start frame can be recomputed
// from its timestamp (docs/architecture/audio.md).
struct OutputAnchor final {
    Time::TimePoint anchor{};
    std::uint64_t firstFrame = 0;
};

// Diagnostics of the output's callbacks, for logs and acceptance only.
struct SdlAudioOutputStats final {
    std::uint64_t callbacks = 0;
    std::uint64_t frames = 0;           // frames handed to SDL
    std::uint32_t minCallbackFrames = 0;
    std::uint32_t maxCallbackFrames = 0;
    Time::Duration maxCallbackGap{};    // longest time between two callbacks' anchors
    std::uint64_t anchorsDropped = 0;   // anchors not logged because the log was full
};

// Plays a mixer on the default playback device. Opening starts SDL's audio
// subsystem (reference-counted with every other owner) and an SDL audio stream
// of 48 kHz stereo PCM16 whose callback, on SDL's audio thread, reads the
// Engine time once and renders the mixer with it as the block's anchor. SDL
// converts to the device's format and follows default-device changes.
//
// The output owns the mixer, so the mixer outlives every callback. Submitters
// created before or after opening keep working; when opening fails they still
// accept plays, which are never heard.
class SdlAudioOutput final {
public:
    // DeviceUnavailable when SDL's audio subsystem or the playback device does
    // not start; the detail carries SDL's message.
    [[nodiscard]] static Base::Result<SdlAudioOutput, AudioError> Open(Mixer&& mixer,
                                                                       const SdlAudioOutputConfig& config = {});

    SdlAudioOutput(SdlAudioOutput&&) noexcept;
    SdlAudioOutput& operator=(SdlAudioOutput&&) noexcept;
    SdlAudioOutput(const SdlAudioOutput&) = delete;
    SdlAudioOutput& operator=(const SdlAudioOutput&) = delete;
    // Stops the stream (no callback runs afterwards), then releases the subsystem.
    ~SdlAudioOutput();

    // The owned mixer, for submitters, the master gain and statistics. Moving
    // the mixer out of it is a Programmer Error the output cannot detect.
    [[nodiscard]] Mixer& OwnedMixer();
    // SDL's audio driver name, for diagnostics only, never for branching.
    [[nodiscard]] const std::string& DriverName() const;
    // The device's block size in frames, as SDL reports it after opening.
    [[nodiscard]] std::uint32_t DeviceSampleFrames() const;
    [[nodiscard]] SdlAudioOutputStats Stats() const;
    // Appends the anchors logged since the last call, oldest first, and returns
    // how many. One thread at a time; the log is a single-producer (the audio
    // thread), single-consumer ring. Returns 0 when anchorLogCapacity is 0.
    std::size_t TakeAnchors(std::vector<OutputAnchor>& out);

private:
    struct Impl;
    explicit SdlAudioOutput(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace Engine::Audio::Sdl
