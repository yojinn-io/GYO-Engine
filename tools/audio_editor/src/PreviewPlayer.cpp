#include "gyo/audio_editor/PreviewPlayer.hpp"

#include "engine/base/Error.hpp"

#include <algorithm>
#include <utility>

namespace Gyo::Tools::AudioEditor {

namespace Audio = Engine::Audio;

Audio::MixerConfig PreviewMixerConfig() noexcept {
    Audio::MixerConfig config;
    config.maxVoices = 2;
    config.maxSubmitters = 1;
    config.queueCapacity = 16;
    config.clipSlotsPerSubmitter = 2;
    config.scheduleDelay = {};
    return config;
}

PreviewPlayer::PreviewPlayer(Audio::Submitter submitter, Audio::Mixer* mixer) noexcept
    : submitter_(std::move(submitter)), mixer_(mixer) {}

void PreviewPlayer::Play(ConvertedClip clip, const Engine::Time::TimePoint at) {
    waiting_ = std::move(clip);
    releasing_ = true;
    Update(at);
}

void PreviewPlayer::Stop(const Engine::Time::TimePoint at) {
    waiting_.reset();
    releasing_ = true;
    Update(at);
}

void PreviewPlayer::Update(const Engine::Time::TimePoint at) {
    if (releasing_) {
        if (!Release(at)) return;
        releasing_ = false;
    }
    if (waiting_.has_value()) TryStart(at);
}

void PreviewPlayer::SetVolume(const float volume) {
    if (mixer_ != nullptr) mixer_->SetMasterGain(std::clamp(volume, 0.0f, 1.0f));
}

Audio::Playhead PreviewPlayer::Position() const noexcept {
    return mixer_ != nullptr ? mixer_->PlayheadOf(shown_) : Audio::Playhead{};
}

bool PreviewPlayer::Release(const Engine::Time::TimePoint at) {
    if (sound_.Valid()) {
        if (!submitter_.Stop(sound_, at)) return false;
        sound_ = {};
    }
    if (clip_.has_value()) {
        if (!submitter_.UnloadClip(*clip_)) return false;
        clip_.reset();
    }
    return true;
}

void PreviewPlayer::TryStart(const Engine::Time::TimePoint at) {
    const ConvertedClip& clip = *waiting_;
    auto loaded = submitter_.LoadClip(Engine::Base::ConstSpan<std::int16_t>(clip.samples.data(), clip.samples.size()),
                                      clip.channels, clip.sampleRate);
    if (!loaded) {
        // A slot comes back once the audio thread has taken the unload sent
        // before; until then the clip waits.
        if (loaded.error().code == Audio::AudioErrorCode::NoClipSlot) return;
        problem_ = Engine::Base::Describe(loaded.error());
        waiting_.reset();
        return;
    }
    clip_ = *loaded;
    waiting_.reset();
    const Audio::SoundHandle sound = submitter_.Play({.clip = *clip_, .at = at});
    if (!sound.Valid()) {
        problem_ = "the preview's queue is full; play again";
        return;
    }
    sound_ = sound;
    shown_ = sound;
    problem_.reset();
}

} // namespace Gyo::Tools::AudioEditor
