#include "gyo/model_viewer/Playback.hpp"

#include <algorithm>
#include <cmath>

namespace Gyo::Tools::ModelViewer {

std::optional<Engine::Model::PlaybackInterval> PlaybackClock::Advance(const double wallSeconds) noexcept {
    if (!playing || !(wallSeconds > 0.0) || !std::isfinite(wallSeconds) || !(durationSeconds > 0.0)) {
        return std::nullopt;
    }
    const double previous = seconds;
    double next = seconds + wallSeconds * (std::isfinite(speed) ? std::max(speed, 0.0) : 0.0);
    if (loop) {
        seconds = std::fmod(next, durationSeconds);
    } else if (next >= durationSeconds) {
        next = durationSeconds;
        seconds = durationSeconds;
        playing = false;
    } else {
        seconds = next;
    }
    if (!(next > previous)) return std::nullopt;
    return Engine::Model::PlaybackInterval{previous, next, durationSeconds, Mode()};
}

void PlaybackClock::Seek(const double clipSeconds) noexcept {
    if (!std::isfinite(clipSeconds)) {
        return;
    }
    seconds = std::clamp(clipSeconds, 0.0, std::max(durationSeconds, 0.0));
}

Engine::Model::PlaybackMode PlaybackClock::Mode() const noexcept {
    return loop ? Engine::Model::PlaybackMode::Loop : Engine::Model::PlaybackMode::Clamp;
}

Engine::Model::ModelResult<void> PreviewPose(const Engine::Model::ModelAsset& model, const std::optional<std::size_t> clip,
                                             const PlaybackClock& clock, Engine::Model::Pose& output) {
    if (!clip) {
        return Engine::Model::MakeDefaultPose(model, output);
    }
    return Engine::Model::SamplePose(model, *clip, clock.seconds, clock.Mode(), output);
}

std::vector<std::size_t> CrossedEvents(const Engine::Model::AnimationEventTrack& track,
                                       const Engine::Model::PlaybackInterval& interval) {
    std::vector<std::size_t> crossed;
    for (std::size_t index = 0; index < track.events.size(); ++index) {
        if (interval.Crossed(track.events[index].seconds)) crossed.push_back(index);
    }
    return crossed;
}

} // namespace Gyo::Tools::ModelViewer
