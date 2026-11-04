#pragma once

#include "gyo/model_viewer/ModelViewerError.hpp"

#include "engine/model/AnimationEvents.hpp"
#include "engine/model/ModelAsset.hpp"

#include <cstddef>
#include <string>
#include <string_view>

// The gyo.anim_events document the viewer edits (docs/model_viewer/architecture.md).
// Whether a document is valid is Engine's to decide (Design Invariant 2): the
// edit operations keep the events of a clip in order and nothing else, and
// CheckedText hands the written text to Engine's parser and binding before
// anything is saved.
namespace Gyo::Tools::ModelViewer {

// The text of a set, as version 1 writes it: the hand-written layout, clips in
// the byte order of their names (the order Engine's parser gives them), every
// seconds value in the shortest form that reads back as the same double.
// Writes what it is given, including what Engine refuses (an empty clip, an
// unsorted clip); CheckedText is the one that refuses.
[[nodiscard]] std::string WriteAnimationEvents(const Engine::Model::AnimationEventSet& set);

// WriteAnimationEvents, once Engine's ParseAnimationEvents accepts the text,
// BindAnimationEvents binds it to model, and the parsed set equals set with its
// clips in name order (every seconds value bit for bit). InvalidEvents with
// Engine's cause (the path of the offending value) otherwise.
[[nodiscard]] Result<std::string, ModelViewerError> CheckedText(const Engine::Model::AnimationEventSet& set,
                                                                const Engine::Model::ModelAsset& model);

// Field-by-field equality, seconds bit for bit.
[[nodiscard]] bool SameEvents(const Engine::Model::AnimationEventSet& left,
                              const Engine::Model::AnimationEventSet& right) noexcept;

// A set with edit operations that keep each clip's events in non-decreasing
// seconds. A clip that loses its last event disappears; a clip gains a track
// with its first event. Moments, names and clip names are not checked here:
// CheckedText refuses what Engine refuses. An index out of range or a clip
// without events is API misuse (GYO_ASSERT).
class EventDocument final {
public:
    explicit EventDocument(Engine::Model::AnimationEventSet set);

    [[nodiscard]] const Engine::Model::AnimationEventSet& Set() const noexcept { return set_; }
    // The track of a clip, or nullptr for a clip without events.
    [[nodiscard]] const Engine::Model::AnimationEventTrack* Track(std::string_view clip) const noexcept;

    // Inserts after the clip's events at or before the moment; returns its index.
    std::size_t Add(std::string_view clip, Engine::Model::AnimationEvent event);
    // Takes the event out and inserts it again at its new moment; returns its new index.
    std::size_t Move(std::string_view clip, std::size_t index, double seconds);
    void Rename(std::string_view clip, std::size_t index, std::string name);
    void Remove(std::string_view clip, std::size_t index);

private:
    Engine::Model::AnimationEventTrack& MutableTrack(std::string_view clip);

    Engine::Model::AnimationEventSet set_;
};

} // namespace Gyo::Tools::ModelViewer
