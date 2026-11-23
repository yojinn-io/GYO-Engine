#include "plugins/sdl_audio/AudioOutput.hpp"

#include "AudioDevice.hpp"

namespace GYO::Plugins::SDLAudio {

namespace {
const std::string kNoDriver;
} // namespace

AudioOutput::AudioOutput(std::unique_ptr<AudioDevice> device, std::optional<AudioOutputFailure> failure)
    : device_(std::move(device)), failure_(std::move(failure)) {}

AudioOutput::~AudioOutput() = default;

const std::string& AudioOutput::DriverName() const noexcept { return device_ ? device_->DriverName() : kNoDriver; }

std::uint32_t AudioOutput::DeviceSampleFrames() const noexcept { return device_ ? device_->DeviceSampleFrames() : 0; }

AudioOutputStats AudioOutput::Stats() const noexcept { return device_ ? device_->Stats() : AudioOutputStats{}; }

void AudioOutput::Stop() noexcept { device_.reset(); }

} // namespace GYO::Plugins::SDLAudio
