#include "RetroFPS/Present/PvpSound.hpp"

#include "RetroFPS/Pvp/ClientLog.hpp"
#include "engine/audio/ClipBank.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/backend/sdl/SdlAudioOutput.hpp"

#include <array>
#include <chrono>
#include <optional>
#include <utility>

namespace fps::pvp {

// Runtime clip slots for the sounds of the local weapon's animation events,
// loaded once the weapon is (after Initialize opened the output).
constexpr std::uint32_t EventSoundSlots = 16;

struct PvpSound::Impl final {
    std::array<Engine::Audio::ClipId, SoundCueCount> clips{};
    std::optional<Engine::Audio::Submitter> submitter;
    std::optional<Engine::Audio::Sdl::SdlAudioOutput> output;
    std::uint64_t played[SoundCueCount]{};
    std::optional<Gyo::Tools::Framework::BoundEventSounds> eventSounds;
    std::vector<std::pair<std::string, Engine::Audio::ClipId>> eventClips; // by sound asset id
    std::uint64_t eventSoundsPlayed{};
    std::uint32_t anchorLogCapacity{};
    bool diagnostics{};
    std::vector<SoundDiagnostics::Cue> cues;
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
    Engine::Audio::MixerConfig mixerConfig;
    mixerConfig.clipSlotsPerSubmitter = EventSoundSlots;
    auto mixer = Engine::Audio::Mixer::Create(mixerConfig, std::move(bank));
    if (!mixer) { error = Engine::Base::Describe(mixer.error()); return false; }
    auto submitter = mixer->CreateSubmitter();
    if (!submitter) { error = Engine::Base::Describe(submitter.error()); return false; }
    impl_->submitter.emplace(std::move(submitter).value());
    Engine::Audio::Sdl::SdlAudioOutputConfig outputConfig;
    outputConfig.anchorLogCapacity = impl_->anchorLogCapacity;
    auto output = Engine::Audio::Sdl::SdlAudioOutput::Open(std::move(mixer).value(), outputConfig);
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

void PvpSound::Play(const SoundCue cue, const Engine::Time::TimePoint at, const StereoGains gains) {
    if (!impl_->submitter) return;
    const auto index = static_cast<std::size_t>(cue);
    if (gains.left <= 0 && gains.right <= 0) return;
    ++impl_->played[index];
    if (impl_->diagnostics) impl_->cues.push_back({cue, {}, at, Engine::Time::Now()});
    static_cast<void>(impl_->submitter->Play({impl_->clips[index], at, gains.left, gains.right}));
}

bool PvpSound::BindEventSounds(const fps::WeaponEventSounds& weapon, const ClipSource& source, std::string& error) {
    impl_->eventSounds.reset();
    // A new binding replaces the old one: its clips go back to the mixer once
    // every play already queued has ended.
    for (const auto& [id, clip] : impl_->eventClips) static_cast<void>(impl_->submitter->UnloadClip(clip));
    impl_->eventClips.clear();
    std::vector<std::pair<std::string, double>> lengths;
    for (const auto& sound : weapon.sounds.sounds) {
        bool known = false;
        for (const auto& [id, seconds] : lengths) known = known || id == sound.soundAssetId;
        if (known) continue;
        std::optional<double> seconds;
        std::string clipError;
        const bool loaded = source(sound.soundAssetId, [&](const SoundClipData& data) {
            seconds = data.channels && data.sampleRate ?
                static_cast<double>(data.interleaved.size() / data.channels) / data.sampleRate : 0.0;
            // Before Initialize there is nothing to load into; the lengths still bind.
            if (!impl_->submitter) return;
            auto id = impl_->submitter->LoadClip(data.interleaved, data.channels, data.sampleRate);
            if (id) impl_->eventClips.emplace_back(sound.soundAssetId, *id);
            else clipError = Engine::Base::Describe(id.error());
        }, error);
        if (!loaded) return false;
        if (!clipError.empty() || !seconds) {
            error = "Sound clip " + sound.soundAssetId + ": " + (clipError.empty() ? "no samples" : clipError);
            return false;
        }
        lengths.emplace_back(sound.soundAssetId, *seconds);
    }
    auto bound = fps::BindWeaponEventSounds(weapon, [&lengths](const std::string_view id) -> std::optional<double> {
        for (const auto& [known, seconds] : lengths)
            if (known == id) return seconds;
        return std::nullopt;
    }, error);
    if (!bound) return false;
    impl_->eventSounds = std::move(bound);
    return true;
}

const Gyo::Tools::Framework::EventSound* PvpSound::FindEventSound(const std::string_view event) const noexcept {
    return impl_->eventSounds ? impl_->eventSounds->Find(event) : nullptr;
}

void PvpSound::PlayEventSound(const Gyo::Tools::Framework::EventSound& sound, const Engine::Time::TimePoint at,
                              const float gain) {
    if (!impl_->submitter) return;
    for (const auto& [id, clip] : impl_->eventClips) {
        if (id != sound.soundAssetId) continue;
        const auto gains = CentredGains(gain);
        if (gains.left <= 0 && gains.right <= 0) return;
        ++impl_->eventSoundsPlayed;
        if (impl_->diagnostics) impl_->cues.push_back({std::nullopt, sound.event, at, Engine::Time::Now()});
        static_cast<void>(impl_->submitter->Play({clip, at, gains.left, gains.right}));
        return;
    }
}

void PvpSound::EnableDiagnostics(const std::uint32_t anchorLogCapacity) {
    impl_->diagnostics = true;
    impl_->anchorLogCapacity = anchorLogCapacity;
}

SoundDiagnostics PvpSound::TakeDiagnostics() {
    SoundDiagnostics result;
    result.cues = std::exchange(impl_->cues, {});
    result.scheduleDelayNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Engine::Audio::MixerConfig{}.scheduleDelay).count();
    if (!impl_->output) return result;
    std::vector<Engine::Audio::Sdl::OutputAnchor> anchors;
    static_cast<void>(impl_->output->TakeAnchors(anchors));
    for (const auto& anchor : anchors) result.anchors.push_back({anchor.anchor, anchor.firstFrame});
    const auto output = impl_->output->Stats();
    const auto mixer = impl_->output->OwnedMixer().Stats();
    result.anchorsDropped = output.anchorsDropped;
    result.submitted = mixer.submitted;
    result.onTime = mixer.playedOnTime;
    result.late = mixer.playedLate;
    result.queueFull = mixer.droppedQueueFull;
    result.noVoice = mixer.droppedNoVoice;
    result.driver = impl_->output->DriverName();
    return result;
}

void PvpSound::LogSummary() const {
    if (!impl_->output) return;
    const auto stats = impl_->output->OwnedMixer().Stats();
    // log-contract: the mixer's balance for this session.
    ClientLog(Engine::Base::LogLevel::Info, "PvP sound summary submitted=%llu on_time=%llu late=%llu queue_full=%llu "
              "no_voice=%llu max_late_ms=%.3f clipped=%llu local_shot=%llu remote_shot=%llu hit_taken=%llu "
              "hit_confirmed=%llu footstep=%llu landing=%llu event_sounds=%llu",
              static_cast<unsigned long long>(stats.submitted), static_cast<unsigned long long>(stats.playedOnTime),
              static_cast<unsigned long long>(stats.playedLate), static_cast<unsigned long long>(stats.droppedQueueFull),
              static_cast<unsigned long long>(stats.droppedNoVoice),
              std::chrono::duration<double, std::milli>(stats.maxLateness).count(),
              static_cast<unsigned long long>(stats.clippedSamples),
              static_cast<unsigned long long>(impl_->played[0]), static_cast<unsigned long long>(impl_->played[1]),
              static_cast<unsigned long long>(impl_->played[2]), static_cast<unsigned long long>(impl_->played[3]),
              static_cast<unsigned long long>(impl_->played[4]), static_cast<unsigned long long>(impl_->played[5]),
              static_cast<unsigned long long>(impl_->eventSoundsPlayed));
}

} // namespace fps::pvp
