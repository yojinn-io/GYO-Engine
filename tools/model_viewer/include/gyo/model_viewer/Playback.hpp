#pragma once

#include "engine/model/Animation.hpp"
#include "engine/model/ModelAsset.hpp"

#include <cstddef>
#include <optional>

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
    void Advance(double wallSeconds) noexcept;
    // Clamped into [0, durationSeconds].
    void Seek(double clipSeconds) noexcept;
    [[nodiscard]] Engine::Model::PlaybackMode Mode() const noexcept;
};

// The pose the preview draws: SamplePose of clip at the clock's seconds, or
// the default pose when there is no clip (a model without clips).
[[nodiscard]] Engine::Model::ModelResult<void> PreviewPose(const Engine::Model::ModelAsset& model,
                                                           std::optional<std::size_t> clip,
                                                           const PlaybackClock& clock, Engine::Model::Pose& output);

} // namespace Gyo::Tools::ModelViewer
