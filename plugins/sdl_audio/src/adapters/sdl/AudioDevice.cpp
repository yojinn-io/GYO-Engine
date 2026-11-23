// The SDL side of AudioDevice (SDL-1: the only file of sdl_audio that includes SDL).
#include "AudioDevice.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "engine/audio/AudioFormat.hpp"

namespace GYO::Plugins::SDLAudio {

namespace {

namespace Audio = Engine::Audio;
namespace Time = Engine::Time;

// Frames rendered per pass inside one callback. A larger request is rendered in
// several passes, each anchored where the one before ended, so the buffer is
// allocated once, at opening.
constexpr std::uint32_t kPassFrames = 4096;
constexpr int kBytesPerFrame = Audio::kChannels * static_cast<int>(sizeof(std::int16_t));

Time::Duration FramesToDuration(const std::uint64_t frames) noexcept {
    return std::chrono::duration_cast<Time::Duration>(
        std::chrono::nanoseconds(static_cast<std::int64_t>(frames * 1'000'000'000ull / Audio::kSampleRate)));
}

AudioOutputFailure Failed(const AudioOutputFailureCode code, const char* message) {
    const char* detail = SDL_GetError();
    return AudioOutputFailure::Make(code, message, detail != nullptr ? detail : "");
}

} // namespace

struct AudioDevice::State final {
    explicit State(Audio::Mixer& played) : mixer(&played), buffer(std::size_t{kPassFrames} * Audio::kChannels) {}

    Audio::Mixer* mixer;              // not owned (audio DI 7)
    std::vector<std::int16_t> buffer; // allocated once, written only by the callback
    SDL_AudioStream* stream = nullptr;
    bool subsystem = false;
    std::string driver;
    std::uint32_t deviceFrames = 0;

    // Written by the callback only (relaxed: display and measurement); read by Stats on any thread.
    std::atomic<std::uint64_t> callbacks{0};
    std::atomic<std::uint64_t> frames{0};
    std::atomic<std::uint32_t> minCallbackFrames{0};
    std::atomic<std::uint32_t> maxCallbackFrames{0};
    std::atomic<std::int64_t> maxGapNs{0};
    Time::TimePoint lastAnchor{}; // the callback's own

    // SDL's audio thread, once per device block. Reads the Engine time base once,
    // as this block's anchor (audio DI 3), and renders the frames SDL asks for.
    // Nothing here throws, allocates, locks or logs (audio DI 7); SDL's own stream
    // queue may allocate inside SDL_PutAudioStreamData.
    static void SDLCALL Callback(void* user, SDL_AudioStream* stream, const int additional, int) noexcept {
        auto& self = *static_cast<State*>(user);
        const Time::TimePoint anchor = Time::Now();
        if (additional <= 0) return;
        const auto wanted = static_cast<std::uint32_t>((additional + kBytesPerFrame - 1) / kBytesPerFrame);
        std::uint32_t done = 0;
        while (done < wanted) {
            const std::uint32_t n = std::min(wanted - done, kPassFrames);
            self.mixer->Render(Engine::Base::Span<std::int16_t>(self.buffer.data(), std::size_t{n} * Audio::kChannels),
                               anchor + FramesToDuration(done));
            SDL_PutAudioStreamData(stream, self.buffer.data(), static_cast<int>(n) * kBytesPerFrame);
            done += n;
        }

        const std::uint64_t count = self.callbacks.load(std::memory_order_relaxed);
        if (count == 0 || wanted < self.minCallbackFrames.load(std::memory_order_relaxed)) {
            self.minCallbackFrames.store(wanted, std::memory_order_relaxed);
        }
        if (wanted > self.maxCallbackFrames.load(std::memory_order_relaxed)) {
            self.maxCallbackFrames.store(wanted, std::memory_order_relaxed);
        }
        if (count != 0) {
            const std::int64_t gap =
                std::chrono::duration_cast<std::chrono::nanoseconds>(anchor - self.lastAnchor).count();
            if (gap > self.maxGapNs.load(std::memory_order_relaxed)) self.maxGapNs.store(gap, std::memory_order_relaxed);
        }
        self.lastAnchor = anchor;
        self.frames.fetch_add(wanted, std::memory_order_relaxed);
        self.callbacks.store(count + 1, std::memory_order_relaxed);
    }
};

Engine::Base::Result<std::unique_ptr<AudioDevice>, AudioOutputFailure> AudioDevice::Open(
    Audio::Mixer& mixer, const AudioDeviceSettings& settings) {
    auto state = std::make_unique<State>(mixer);
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        return Engine::Base::Err(Failed(AudioOutputFailureCode::SubsystemUnavailable, "SDL audio did not start"));
    }
    state->subsystem = true;
    std::unique_ptr<AudioDevice> device(new AudioDevice(std::move(state))); // from here the destructor cleans up
    State& self = *device->state_;

    const bool hinted = settings.deviceSampleFrames != 0;
    if (hinted) {
        SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, std::to_string(settings.deviceSampleFrames).c_str());
    }
    const SDL_AudioSpec spec{SDL_AUDIO_S16, Audio::kChannels, static_cast<int>(Audio::kSampleRate)};
    self.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &State::Callback, &self);
    if (hinted) SDL_ResetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);
    if (self.stream == nullptr) {
        return Engine::Base::Err(Failed(AudioOutputFailureCode::DeviceUnavailable, "the playback device did not open"));
    }

    SDL_AudioSpec deviceSpec{};
    int deviceFrames = 0;
    if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(self.stream), &deviceSpec, &deviceFrames) &&
        deviceFrames > 0) {
        self.deviceFrames = static_cast<std::uint32_t>(deviceFrames);
    }
    const char* driver = SDL_GetCurrentAudioDriver();
    self.driver = driver != nullptr ? driver : "";
    // A device stream opens paused.
    if (!SDL_ResumeAudioStreamDevice(self.stream)) {
        return Engine::Base::Err(Failed(AudioOutputFailureCode::DeviceNotStarted, "the playback device did not start"));
    }
    return device;
}

AudioDevice::AudioDevice(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {}

AudioDevice::~AudioDevice() {
    // Destroying the stream unbinds it from the device under SDL's lock, so no
    // callback is running or will run once it returns.
    if (state_->stream != nullptr) SDL_DestroyAudioStream(state_->stream);
    if (state_->subsystem) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

const std::string& AudioDevice::DriverName() const noexcept { return state_->driver; }

std::uint32_t AudioDevice::DeviceSampleFrames() const noexcept { return state_->deviceFrames; }

AudioOutputStats AudioDevice::Stats() const noexcept {
    const State& self = *state_;
    return AudioOutputStats{
        self.callbacks.load(std::memory_order_relaxed),
        self.frames.load(std::memory_order_relaxed),
        self.minCallbackFrames.load(std::memory_order_relaxed),
        self.maxCallbackFrames.load(std::memory_order_relaxed),
        std::chrono::duration_cast<Time::Duration>(std::chrono::nanoseconds(self.maxGapNs.load(std::memory_order_relaxed))),
    };
}

} // namespace GYO::Plugins::SDLAudio
