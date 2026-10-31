#pragma once

#include "engine/model/ModelAsset.hpp"
#include "engine/model/ModelError.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Model {

// gyo.anim_events version 1: named moments on the clips of one model
// (docs/architecture/animation-events.md). The module keeps the names opaque;
// what an event means, and what it triggers, belongs to the consumer.

inline constexpr std::string_view AnimationEventsSchema = "gyo.anim_events";
inline constexpr int AnimationEventsVersion = 1;
inline constexpr std::size_t MaxAnimationEventNameLength = 64;

struct AnimationEvent final {
    // The clip's own seconds, in [0, the clip's duration].
    double seconds{};
    std::string name;
};

struct AnimationEventTrack final {
    std::string clip;                   // the exact clip name in the model
    std::vector<AnimationEvent> events; // nonempty, in non-decreasing seconds
};

struct AnimationEventSet final {
    std::string modelAssetId;
    std::vector<AnimationEventTrack> tracks; // one per clip, ordered by clip name
};

// Validates a document against every rule that does not need the model.
// Anything the contract does not allow is an InvalidAnimationEvents error with
// the offending path as its detail; nothing is ignored or repaired.
[[nodiscard]] ModelResult<AnimationEventSet> ParseAnimationEvents(std::string_view text);

// The events of a set resolved against the model it names. The caller checks
// that the model is the one modelAssetId names; the module does not know IDs.
struct BoundAnimationEvents final {
    // Indexed by the model's clip index; empty for a clip without events.
    std::vector<std::vector<AnimationEvent>> byClip;

    [[nodiscard]] std::span<const AnimationEvent> ForClip(std::size_t clipIndex) const noexcept;
};

// Every named clip must exist in the model and every event must lie within
// its clip's duration; otherwise binding fails as a whole.
[[nodiscard]] ModelResult<BoundAnimationEvents> BindAnimationEvents(
    const AnimationEventSet& events, const ModelAsset& model);

} // namespace Engine::Model
