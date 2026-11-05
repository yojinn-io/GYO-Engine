#pragma once

#include "gyo/audio_editor/Conversion.hpp"

#include "engine/audio/AudioError.hpp"
#include "engine/audio/ClipBank.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <optional>
#include <string>

namespace Gyo::Tools::AudioEditor {

// The mixer the preview plays on: one voice at a time, two clip slots (the
// clip playing and the next one), and no schedule delay, so a stop and an
// unload take effect in the block that takes them and a slot comes back
// after one block.
[[nodiscard]] Engine::Audio::MixerConfig PreviewMixerConfig() noexcept;

// Plays the clip RenderClip made, as it is (Design Invariant 6): the samples
// go to LoadClip unchanged and play at gain 1; the preview volume is the
// mixer's master gain and never reaches the clip. One clip at a time: a new
// play stops and unloads the one before. All members on the thread that
// created the submitter (the editor's main thread).
//
// The mixer is the authority for where a sound is (GYO::Audio Design
// Invariant 4): the player keeps no position and decides nothing from one;
// Position is for display only.
class PreviewPlayer final {
public:
    // mixer is the one the submitter belongs to, or null when no output opened
    // (plays are then sent and never heard, and Position is NotPlaying).
    PreviewPlayer(Engine::Audio::Submitter submitter, Engine::Audio::Mixer* mixer) noexcept;

    // Stops and unloads what plays, then plays clip from its start at `at`.
    // When no clip slot is free yet (the audio thread has not handed one back)
    // the clip waits and Update tries again.
    void Play(ConvertedClip clip, Engine::Time::TimePoint at);
    // Stops and unloads what plays, and drops a clip that waits.
    void Stop(Engine::Time::TimePoint at);
    // Once per editor frame: sends what a full queue or a missing slot held back.
    void Update(Engine::Time::TimePoint at);
    // 0 to 1, applied by the mixer at playback only.
    void SetVolume(float volume);

    // A clip waits for a slot.
    [[nodiscard]] bool Waiting() const noexcept { return waiting_.has_value(); }
    // The sound last played (stopped or not), invalid before the first.
    [[nodiscard]] Engine::Audio::SoundHandle Current() const noexcept { return shown_; }
    // Display only: as Mixer::PlayheadOf.
    [[nodiscard]] Engine::Audio::Playhead Position() const noexcept;
    // Why the last play did not start, cleared by the next one that does.
    [[nodiscard]] const std::optional<std::string>& Problem() const noexcept { return problem_; }

private:
    // Sends the stop and the unload of the clip played last; false while the
    // queue is full (Update sends them again).
    bool Release(Engine::Time::TimePoint at);
    void TryStart(Engine::Time::TimePoint at);

    Engine::Audio::Submitter submitter_;
    Engine::Audio::Mixer* mixer_;
    Engine::Audio::SoundHandle sound_;           // the play to stop; cleared once its stop is queued
    Engine::Audio::SoundHandle shown_;           // the play last started; Position shows it
    std::optional<Engine::Audio::ClipId> clip_;  // loaded and not yet unloaded
    bool releasing_ = false;                     // a play or stop asked to release sound_ and clip_
    std::optional<ConvertedClip> waiting_;
    std::optional<std::string> problem_;
};

} // namespace Gyo::Tools::AudioEditor
