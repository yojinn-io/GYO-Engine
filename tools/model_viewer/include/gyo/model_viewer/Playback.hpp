#pragma once

#include "engine/model/Animation.hpp"
#include "engine/model/AnimationEvents.hpp"
#include "engine/model/ModelAsset.hpp"

#include <cstddef>
#include <optional>
#include <vector>

// Playback of one clip in the preview (Design Invariant 5): wall time becomes
// clip seconds through the speed, and nothing else. The pose comes from
// Engine's SamplePose at those seconds.
namespace Gyo::Tools::ModelViewer {

struct PlaybackClock final {
    double durationSeconds{};
    double speed{1.0}; // clip seconds per wall second; below 0 or not finite counts as 0
    bool loop{true};
    bool playing{};
    double seconds{}; // clip seconds, in [0, durationSeconds]

    // Moves seconds on by wallSeconds * speed while playing. A looping clock
    // wraps into [0, durationSeconds); a clamped one stops at the end. A
    // negative or non-finite wall time, or a clip of no length, moves nothing.
    // Returns the clip seconds it went through, unwrapped (Engine's
    // PlaybackInterval, whose Crossed says which events they pass), or none
    // when it did not move.
    std::optional<Engine::Model::PlaybackInterval> Advance(double wallSeconds) noexcept;
    // Clamped into [0, durationSeconds].
    void Seek(double clipSeconds) noexcept;
    [[nodiscard]] Engine::Model::PlaybackMode Mode() const noexcept;
};

// The pose the preview draws: SamplePose of clip at the clock's seconds, or
// the default pose when there is no clip (a model without clips).
[[nodiscard]] Engine::Model::ModelResult<void> PreviewPose(const Engine::Model::ModelAsset& model,
                                                           std::optional<std::size_t> clip,
                                                           const PlaybackClock& clock, Engine::Model::Pose& output);

// The events of track that interval passes, by index: Engine's
// PlaybackInterval::Crossed only (each once, also across a loop boundary).
[[nodiscard]] std::vector<std::size_t> CrossedEvents(const Engine::Model::AnimationEventTrack& track,
                                                     const Engine::Model::PlaybackInterval& interval);

} // namespace Gyo::Tools::ModelViewer
