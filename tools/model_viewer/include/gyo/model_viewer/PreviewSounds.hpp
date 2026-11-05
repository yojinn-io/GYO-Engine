#pragma once

#include "gyo/model_viewer/EventSoundFiles.hpp"
#include "gyo/model_viewer/ModelContent.hpp"
#include "gyo/model_viewer/ModelViewerError.hpp"

#include <memory>
#include <string_view>

// The preview's sounds (-event-sounds): Engine's mixer on its SDL audio
// output. The sounds play at the times ScheduleSounds and PlayClipSounds give;
// the viewer has no schedule of its own (Design Invariant 5).
namespace Gyo::Tools::ModelViewer {

class PreviewSounds final : public SoundSink {
public:
    // PreviewFailed when the mixer cannot be made or the audio device does not open.
    [[nodiscard]] static Result<std::unique_ptr<PreviewSounds>, ModelViewerError> Open();
    ~PreviewSounds() override;

    // Copies the clip in once per asset id; PreviewFailed when the mixer refuses it.
    [[nodiscard]] Result<void, ModelViewerError> Load(const LoadedSound& sound);
    [[nodiscard]] bool Loaded(std::string_view assetId) const;
    // A clip that was not loaded is not played.
    void Play(const ScheduledSound& sound, Engine::Time::TimePoint at) override;
    // Ends every sound sent so far, now.
    void StopAll() override;
    // Stops everything and unloads every clip (the sounds on disk may have changed).
    void Clear();

private:
    struct Impl;
    explicit PreviewSounds(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace Gyo::Tools::ModelViewer
