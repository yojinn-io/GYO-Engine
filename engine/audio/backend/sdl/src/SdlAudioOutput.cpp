#include "engine/audio/backend/sdl/SdlAudioOutput.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "engine/audio/AudioFormat.hpp"
#include "engine/base/Assert.hpp"

namespace Engine::Audio::Sdl {

namespace {

// Frames rendered per pass inside one callback; a larger request is rendered in
// several passes, each anchored where the previous one ended.
constexpr std::uint32_t kPassFrames = 4096;
constexpr int kBytesPerFrame = kChannels * static_cast<int>(sizeof(std::int16_t));

Time::Duration FramesToDuration(const std::uint64_t frames) noexcept {
    return std::chrono::duration_cast<Time::Duration>(
        std::chrono::nanoseconds(static_cast<std::int64_t>(frames * 1'000'000'000ull / kSampleRate)));
}

AudioError Unavailable(const char* message) {
    const char* detail = SDL_GetError();
    return AudioError::Make(AudioErrorCode::DeviceUnavailable, message, detail != nullptr ? detail : "");
}

} // namespace

struct SdlAudioOutput::Impl final {
    Impl(Mixer&& owned, const std::uint32_t anchorCapacity)
        : mixer(std::move(owned)), buffer(std::size_t{kPassFrames} * kChannels), anchors(anchorCapacity) {}

    Mixer mixer;
    std::vector<std::int16_t> buffer; // allocated once, written only by the callback
    SDL_AudioStream* stream = nullptr;
    bool subsystem = false;
    std::string driver;
    std::uint32_t deviceFrames = 0;

    // Written by the callback only; read by Stats on any thread.
    std::atomic<std::uint64_t> callbacks{0};
    std::atomic<std::uint64_t> frames{0};
    std::atomic<std::uint32_t> minCallbackFrames{0};
    std::atomic<std::uint32_t> maxCallbackFrames{0};
    std::atomic<std::int64_t> maxGapNs{0};
    Time::TimePoint lastAnchor{}; // the callback's own

    // The anchor log: the callback owns tail, TakeAnchors owns head (release on
    // publish, acquire on read, as in the mixer's submitter rings).
    std::vector<OutputAnchor> anchors; // allocated once
    std::atomic<std::uint64_t> anchorTail{0};
    std::atomic<std::uint64_t> anchorHead{0};
    std::atomic<std::uint64_t> anchorsDropped{0};

    void LogAnchor(const Time::TimePoint anchor, const std::uint64_t firstFrame) noexcept {
        const std::uint64_t capacity = anchors.size();
        if (capacity == 0) return;
        const std::uint64_t tail = anchorTail.load(std::memory_order_relaxed);
        if (tail - anchorHead.load(std::memory_order_acquire) >= capacity) {
            anchorsDropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        anchors[tail % capacity] = OutputAnchor{anchor, firstFrame};
        anchorTail.store(tail + 1, std::memory_order_release);
    }

    // SDL's audio thread. Reads the Engine time once and renders the frames SDL
    // asks for. Nothing here throws, allocates or locks (SDL's own stream
    // queue may allocate inside SDL_PutAudioStreamData).
    static void SDLCALL Callback(void* user, SDL_AudioStream* stream, const int additional, int) noexcept {
        auto& self = *static_cast<Impl*>(user);
        const Time::TimePoint anchor = Time::Now();
        if (additional <= 0) return;
        const auto wanted = static_cast<std::uint32_t>((additional + kBytesPerFrame - 1) / kBytesPerFrame);
        // Only this thread renders, so the count read here is this block's first frame.
        self.LogAnchor(anchor, self.mixer.Stats().renderedFrames);

        std::uint32_t done = 0;
        while (done < wanted) {
            const std::uint32_t n = std::min(wanted - done, kPassFrames);
            self.mixer.Render(Base::Span<std::int16_t>(self.buffer.data(), std::size_t{n} * kChannels),
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
            const std::int64_t gap = std::chrono::duration_cast<std::chrono::nanoseconds>(anchor - self.lastAnchor).count();
            if (gap > self.maxGapNs.load(std::memory_order_relaxed)) self.maxGapNs.store(gap, std::memory_order_relaxed);
        }
        self.lastAnchor = anchor;
        self.frames.fetch_add(wanted, std::memory_order_relaxed);
        self.callbacks.store(count + 1, std::memory_order_relaxed);
    }
};

Base::Result<SdlAudioOutput, AudioError> SdlAudioOutput::Open(Mixer&& mixer, const SdlAudioOutputConfig& config) {
    auto impl = std::make_unique<Impl>(std::move(mixer), config.anchorLogCapacity);
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return Base::Err(Unavailable("SDL audio did not start"));
    impl->subsystem = true;
    SdlAudioOutput output(std::move(impl)); // from here the destructor releases the subsystem

    const bool hinted = config.deviceSampleFrames != 0;
    if (hinted) {
        SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, std::to_string(config.deviceSampleFrames).c_str());
    }
    const SDL_AudioSpec spec{SDL_AUDIO_S16, kChannels, static_cast<int>(kSampleRate)};
    Impl& self = *output.impl_;
    self.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Impl::Callback, &self);
    if (hinted) SDL_ResetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);
    if (self.stream == nullptr) return Base::Err(Unavailable("the playback device did not open"));

    SDL_AudioSpec deviceSpec{};
    int deviceFrames = 0;
    if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(self.stream), &deviceSpec, &deviceFrames) &&
        deviceFrames > 0) {
        self.deviceFrames = static_cast<std::uint32_t>(deviceFrames);
    }
    const char* driver = SDL_GetCurrentAudioDriver();
    self.driver = driver != nullptr ? driver : "";
    // A device stream opens paused.
    if (!SDL_ResumeAudioStreamDevice(self.stream)) return Base::Err(Unavailable("the playback device did not start"));
    return output;
}

SdlAudioOutput::SdlAudioOutput(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
SdlAudioOutput::SdlAudioOutput(SdlAudioOutput&&) noexcept = default;

SdlAudioOutput& SdlAudioOutput::operator=(SdlAudioOutput&& other) noexcept {
    if (this != &other) {
        SdlAudioOutput released(std::move(*this));
        impl_ = std::move(other.impl_);
    }
    return *this;
}

SdlAudioOutput::~SdlAudioOutput() {
    if (!impl_) return;
    // Destroying the stream unbinds it from the device under SDL's lock, so no
    // callback is running or will run once it returns.
    if (impl_->stream != nullptr) SDL_DestroyAudioStream(impl_->stream);
    if (impl_->subsystem) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

Mixer& SdlAudioOutput::OwnedMixer() {
    GYO_ASSERT(impl_ != nullptr);
    return impl_->mixer;
}

const std::string& SdlAudioOutput::DriverName() const {
    GYO_ASSERT(impl_ != nullptr);
    return impl_->driver;
}

std::uint32_t SdlAudioOutput::DeviceSampleFrames() const {
    GYO_ASSERT(impl_ != nullptr);
    return impl_->deviceFrames;
}

SdlAudioOutputStats SdlAudioOutput::Stats() const {
    GYO_ASSERT(impl_ != nullptr);
    return SdlAudioOutputStats{
        impl_->callbacks.load(std::memory_order_relaxed),
        impl_->frames.load(std::memory_order_relaxed),
        impl_->minCallbackFrames.load(std::memory_order_relaxed),
        impl_->maxCallbackFrames.load(std::memory_order_relaxed),
        std::chrono::duration_cast<Time::Duration>(
            std::chrono::nanoseconds(impl_->maxGapNs.load(std::memory_order_relaxed))),
        impl_->anchorsDropped.load(std::memory_order_relaxed),
    };
}

std::size_t SdlAudioOutput::TakeAnchors(std::vector<OutputAnchor>& out) {
    GYO_ASSERT(impl_ != nullptr);
    Impl& self = *impl_;
    const std::uint64_t capacity = self.anchors.size();
    if (capacity == 0) return 0;
    std::uint64_t head = self.anchorHead.load(std::memory_order_relaxed);
    const std::uint64_t tail = self.anchorTail.load(std::memory_order_acquire);
    const std::size_t taken = static_cast<std::size_t>(tail - head);
    for (; head != tail; ++head) out.push_back(self.anchors[head % capacity]);
    self.anchorHead.store(head, std::memory_order_release);
    return taken;
}

} // namespace Engine::Audio::Sdl
