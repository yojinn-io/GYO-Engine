#include "gyo/model_viewer/PreviewSounds.hpp"

#include "engine/audio/ClipBank.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/backend/sdl/SdlAudioOutput.hpp"
#include "engine/base/Error.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>

namespace Gyo::Tools::ModelViewer {

struct PreviewSounds::Impl final {
    std::optional<Engine::Audio::Sdl::SdlAudioOutput> output;
    std::optional<Engine::Audio::Submitter> submitter;
    std::map<std::string, Engine::Audio::ClipId, std::less<>> clips;
};

PreviewSounds::PreviewSounds(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
PreviewSounds::~PreviewSounds() = default;

Result<std::unique_ptr<PreviewSounds>, ModelViewerError> PreviewSounds::Open() {
    Engine::Audio::MixerConfig config;
    config.clipSlotsPerSubmitter = 64;
    auto mixer = Engine::Audio::Mixer::Create(config, Engine::Audio::ClipBank{});
    if (!mixer) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::PreviewFailed, "cannot make the mixer",
                                                        Engine::Base::CauseDetail(mixer.error())));
    }
    auto impl = std::make_unique<Impl>();
    auto submitter = mixer.value().CreateSubmitter();
    if (!submitter) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::PreviewFailed, "cannot make a submitter",
                                                        Engine::Base::CauseDetail(submitter.error())));
    }
    impl->submitter.emplace(std::move(submitter.value()));
    auto output = Engine::Audio::Sdl::SdlAudioOutput::Open(std::move(mixer.value()));
    if (!output) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::PreviewFailed, "cannot open the audio device",
                                                        Engine::Base::CauseDetail(output.error())));
    }
    impl->output.emplace(std::move(output.value()));
    return std::unique_ptr<PreviewSounds>(new PreviewSounds(std::move(impl)));
}

Result<void, ModelViewerError> PreviewSounds::Load(const LoadedSound& sound) {
    if (Loaded(sound.assetId)) return {};
    auto clip = impl_->submitter->LoadClip(Engine::Base::ConstSpan<std::int16_t>(sound.pcm16.data(), sound.pcm16.size()),
                                           sound.channels, sound.sampleRate);
    if (!clip) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::PreviewFailed, "the mixer refuses the sound",
                                                        Engine::Base::CauseDetail(clip.error(), sound.assetId)));
    }
    impl_->clips.emplace(sound.assetId, clip.value());
    return {};
}

bool PreviewSounds::Loaded(const std::string_view assetId) const { return impl_->clips.find(assetId) != impl_->clips.end(); }

void PreviewSounds::Play(const ScheduledSound& sound, const Engine::Time::TimePoint at) {
    const auto found = impl_->clips.find(sound.soundAssetId);
    if (found == impl_->clips.end()) return;
    static_cast<void>(impl_->submitter->Play({found->second, at, sound.gain, sound.gain}));
}

void PreviewSounds::StopAll() { static_cast<void>(impl_->submitter->StopAll(Engine::Time::Now())); }

void PreviewSounds::Clear() {
    StopAll();
    // An unload queued behind the stop frees the slot once no voice reads it.
    for (const auto& [id, clip] : impl_->clips) static_cast<void>(impl_->submitter->UnloadClip(clip));
    impl_->clips.clear();
}

} // namespace Gyo::Tools::ModelViewer
