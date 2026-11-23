#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "engine/audio/Mixer.hpp"
#include "engine/base/Result.hpp"
#include "plugins/sdl_audio/AudioOutput.hpp"

// The adapter's interface for the rest of the Plugin, in GYO's words (SDL-1): it
// lives outside src/adapters/sdl/, and src/adapters/sdl/AudioDevice.cpp
// implements it with SDL.

namespace GYO::Plugins::SDLAudio {

struct AudioDeviceSettings final {
    std::uint32_t deviceSampleFrames = 0; // 0: the platform's choice
};

// The default playback device, playing one mixer from its own audio thread: each
// callback reads the Engine time base once and renders the mixer with it as the
// block's anchor (audio DI 3, SDL-3). It holds no share of the mixer (audio DI 7):
// the mixer's holder keeps it alive and unmoved until this is destroyed.
class AudioDevice final {
public:
    // Opens and starts the device. The mixer must outlive the returned device.
    [[nodiscard]] static Engine::Base::Result<std::unique_ptr<AudioDevice>, AudioOutputFailure> Open(
        Engine::Audio::Mixer& mixer, const AudioDeviceSettings& settings);

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;
    // Stops the device (no callback runs once it returns), then releases the subsystem.
    ~AudioDevice();

    [[nodiscard]] const std::string& DriverName() const noexcept;
    [[nodiscard]] std::uint32_t DeviceSampleFrames() const noexcept;
    [[nodiscard]] AudioOutputStats Stats() const noexcept;

    struct State;

private:
    explicit AudioDevice(std::unique_ptr<State> state) noexcept;
    std::unique_ptr<State> state_;
};

} // namespace GYO::Plugins::SDLAudio
