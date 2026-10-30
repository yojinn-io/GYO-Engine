#include "RetroFPS/Pvp/PvpSound.hpp"

#include "RetroFPS/Pvp/ClientLog.hpp"
#include "engine/audio/ClipBank.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/backend/sdl/SdlAudioOutput.hpp"

#include <array>
#include <chrono>
#include <optional>
#include <utility>

namespace fps::pvp {

struct PvpSound::Impl final {
    std::array<Engine::Audio::ClipId, SoundCueCount> clips{};
    std::optional<Engine::Audio::Submitter> submitter;
    std::optional<Engine::Audio::Sdl::SdlAudioOutput> output;
    std::uint64_t played[SoundCueCount]{};
    double seconds[SoundCueCount]{};
};

PvpSound::PvpSound() : impl_(std::make_unique<Impl>()) {}
PvpSound::~PvpSound() = default;

bool PvpSound::Initialize(const SoundSettings& settings, const ClipSource& source, std::string& error) {
    settings_ = settings;
    Engine::Audio::ClipBank bank;
    for (std::size_t i = 0; i < SoundCueCount; ++i) {
        // Cues may share a clip; each cue keeps the id of its first registration.
        bool shared = false;
        for (std::size_t j = 0; j < i; ++j) {
            if (settings.cues[j].clip == settings.cues[i].clip) {
                impl_->clips[i] = impl_->clips[j];
                impl_->seconds[i] = impl_->seconds[j];
                shared = true;
                break;
            }
        }
        if (shared) continue;
        bool added = false;
        std::string clipError;
        const bool loaded = source(settings.cues[i].clip, [&](const SoundClipData& data) {
            auto id = bank.Add(data.interleaved, data.channels, data.sampleRate);
            if (id) {
                impl_->clips[i] = *id;
                impl_->seconds[i] = static_cast<double>(data.interleaved.size() / data.channels) / data.sampleRate;
                added = true;
            } else {
                clipError = Engine::Base::Describe(id.error());
            }
        }, error);
        if (!loaded) return false;
        if (!added) {
            error = "Sound clip " + settings.cues[i].clip + ": " + clipError;
            return false;
        }
    }
    auto mixer = Engine::Audio::Mixer::Create(Engine::Audio::MixerConfig{}, std::move(bank));
    if (!mixer) { error = Engine::Base::Describe(mixer.error()); return false; }
    auto submitter = mixer->CreateSubmitter();
    if (!submitter) { error = Engine::Base::Describe(submitter.error()); return false; }
    impl_->submitter.emplace(std::move(submitter).value());
    auto output = Engine::Audio::Sdl::SdlAudioOutput::Open(std::move(mixer).value());
    if (!output) {
        // log-contract: the one line saying this Client plays no sound, and why.
        ClientLog(Engine::Base::LogLevel::Warning, "PvP sound unavailable error=\"%s\"",
                  Engine::Base::Describe(output.error()).c_str());
        return true;
    }
    impl_->output.emplace(std::move(output).value());
    ClientLog(Engine::Base::LogLevel::Info, "PvP sound started driver=%s device_frames=%u schedule_delay_ms=%lld",
              impl_->output->DriverName().c_str(), impl_->output->DeviceSampleFrames(),
              static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                  Engine::Audio::MixerConfig{}.scheduleDelay).count()));
    return true;
}

double PvpSound::ClipSeconds(const SoundCue cue) const noexcept {
    const auto index = static_cast<std::size_t>(cue);
    return index < SoundCueCount ? impl_->seconds[index] : 0;
}

void PvpSound::Play(const SoundCue cue, const Engine::Time::TimePoint at, const StereoGains gains) {
    if (!impl_->submitter) return;
    const auto index = static_cast<std::size_t>(cue);
    if (gains.left <= 0 && gains.right <= 0) return;
    ++impl_->played[index];
    static_cast<void>(impl_->submitter->Play({impl_->clips[index], at, gains.left, gains.right}));
}

void PvpSound::LogSummary() const {
    if (!impl_->output) return;
    const auto stats = impl_->output->OwnedMixer().Stats();
    // log-contract: the mixer's balance for this session.
    ClientLog(Engine::Base::LogLevel::Info, "PvP sound summary submitted=%llu on_time=%llu late=%llu queue_full=%llu "
              "no_voice=%llu max_late_ms=%.3f clipped=%llu local_shot=%llu remote_shot=%llu hit_taken=%llu "
              "hit_confirmed=%llu reload=%llu footstep=%llu landing=%llu",
              static_cast<unsigned long long>(stats.submitted), static_cast<unsigned long long>(stats.playedOnTime),
              static_cast<unsigned long long>(stats.playedLate), static_cast<unsigned long long>(stats.droppedQueueFull),
              static_cast<unsigned long long>(stats.droppedNoVoice),
              std::chrono::duration<double, std::milli>(stats.maxLateness).count(),
              static_cast<unsigned long long>(stats.clippedSamples),
              static_cast<unsigned long long>(impl_->played[0]), static_cast<unsigned long long>(impl_->played[1]),
              static_cast<unsigned long long>(impl_->played[2]), static_cast<unsigned long long>(impl_->played[3]),
              static_cast<unsigned long long>(impl_->played[4]), static_cast<unsigned long long>(impl_->played[5]),
              static_cast<unsigned long long>(impl_->played[6]));
}

} // namespace fps::pvp
