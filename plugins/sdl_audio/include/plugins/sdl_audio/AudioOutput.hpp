#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "engine/audio/Mixer.hpp"
#include "engine/base/Error.hpp"
#include "engine/time/MonotonicClock.hpp"

// The audio output of the sdl_audio Plugin (docs/architecture/target/v2/sdl.zh-Hant.md,
// sdl_audio; SDL declaration section 7): what a product gives it and what it puts
// in the Product's buffer. In GYO's words: no SDL type appears here (SDL-1).

namespace GYO::Plugins::SDLAudio {

// A product option (ProductDescription::options). Without it the defaults hold.
struct AudioOutputOptions final {
    // The mixer the output creates when the product gives none (AudioOutputMixer).
    Engine::Audio::MixerConfig mixer{};
    // Frames per device block asked of the device; 0 keeps the platform's choice.
    // A hint: the device may choose another size (AudioOutput::DeviceSampleFrames).
    std::uint32_t deviceSampleFrames = 0;
};

// A product option: the product's own mixer, which the output plays instead of
// creating one (it then puts no Mixer in the Product's buffer). The option lives
// until after the Product's buffer, so the mixer outlives the device either way;
// the output holds no share of it on the audio thread.
struct AudioOutputMixer final {
    std::shared_ptr<Engine::Audio::Mixer> mixer;
};

// Why the device does not play. The Product runs regardless: plays are accepted
// and counted, never heard. Zero is not a valid code.
enum class AudioOutputFailureCode : std::uint8_t {
    // The platform's audio subsystem did not start (no audio driver).
    SubsystemUnavailable = 1,
    // The default playback device did not open.
    DeviceUnavailable,
    // The device opened but did not start playing.
    DeviceNotStarted,
};

[[nodiscard]] constexpr const char* ToString(const AudioOutputFailureCode code) noexcept {
    switch (code) {
    case AudioOutputFailureCode::SubsystemUnavailable: return "SubsystemUnavailable";
    case AudioOutputFailureCode::DeviceUnavailable: return "DeviceUnavailable";
    case AudioOutputFailureCode::DeviceNotStarted: return "DeviceNotStarted";
    }
    return "Unknown";
}

// E2 (naming rule N25): this output did not start; the rest of the Product is
// healthy. The detail carries the platform's message.
using AudioOutputFailure = Engine::Base::Error<AudioOutputFailureCode>;
static_assert(Engine::Base::CodedError<AudioOutputFailure>);

// The device's callbacks, for display and measurement only (never for decisions).
struct AudioOutputStats final {
    std::uint64_t callbacks = 0;
    std::uint64_t frames = 0; // handed to the device
    std::uint32_t minCallbackFrames = 0;
    std::uint32_t maxCallbackFrames = 0;
    // The longest time between two callbacks' anchors: with the consumer's frame
    // time, what the mixer's scheduleDelay has to cover.
    Engine::Time::Duration maxCallbackGap{};
};

class AudioDevice;

// What sdl_audio puts in the Product's buffer: whether the device plays, read by
// the author, who decides what follows (a silent game is still a game). Created
// in the Plugin's Configure; the device stops in its OnDestroy, before the
// Product's buffer, so the mixer outlives every callback (audio DI 7).
class AudioOutput final {
public:
    // Made by the Plugin only (AudioDevice is the Plugin's own type).
    AudioOutput(std::unique_ptr<AudioDevice> device, std::optional<AudioOutputFailure> failure);
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    ~AudioOutput();

    // Null while the device plays or after it stopped; why it never played otherwise.
    [[nodiscard]] const AudioOutputFailure* Failure() const noexcept { return failure_ ? &*failure_ : nullptr; }
    // Whether the device plays now: false when it failed and after Stop.
    [[nodiscard]] bool Playing() const noexcept { return device_ != nullptr; }
    // The platform's audio driver name, for diagnostics only, never for branching;
    // empty when the device does not play.
    [[nodiscard]] const std::string& DriverName() const noexcept;
    // The device's block size in frames as the platform reports it; 0 when not playing.
    [[nodiscard]] std::uint32_t DeviceSampleFrames() const noexcept;
    // Zeros when the device does not play.
    [[nodiscard]] AudioOutputStats Stats() const noexcept;
    // Stops the device: no callback runs once it returns. The Plugin calls it in
    // its OnDestroy; calling it again does nothing.
    void Stop() noexcept;

private:
    std::unique_ptr<AudioDevice> device_;
    std::optional<AudioOutputFailure> failure_;
};

} // namespace GYO::Plugins::SDLAudio
