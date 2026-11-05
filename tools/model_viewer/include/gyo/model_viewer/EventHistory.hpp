#pragma once

#include "gyo/model_viewer/EventDocument.hpp"

#include "engine/model/AnimationEvents.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The events being edited in the window, with Undo and Redo (judgment E2 of
// -events): each edit is one step, a step back gives the events exactly as
// they were before it, and the document is unsaved whenever it is not the
// state last saved.
namespace Gyo::Tools::ModelViewer {

class EventHistory final {
public:
    // saved: whether the events are what a file holds (an opened file) or
    // nothing on disk yet (a new document, unsaved in every state).
    explicit EventHistory(Engine::Model::AnimationEventSet events, bool saved = true);

    [[nodiscard]] const EventDocument& Document() const noexcept { return document_; }

    // The EventDocument operations, each one undo step; the redo steps go.
    std::size_t Add(std::string_view clip, Engine::Model::AnimationEvent event);
    // No step when the seconds do not change.
    std::size_t Move(std::string_view clip, std::size_t index, double seconds);
    // No step when the name does not change.
    void Rename(std::string_view clip, std::size_t index, std::string name);
    void Remove(std::string_view clip, std::size_t index);

    [[nodiscard]] bool CanUndo() const noexcept { return current_ > 0; }
    [[nodiscard]] bool CanRedo() const noexcept { return current_ + 1 < states_.size(); }
    bool Undo();
    bool Redo();

    // The current events are what the file now holds.
    void MarkSaved() noexcept { saved_ = current_; }
    [[nodiscard]] bool Unsaved() const noexcept { return saved_ != current_; }

private:
    void Record();

    EventDocument document_;
    std::vector<Engine::Model::AnimationEventSet> states_;
    std::size_t current_{};
    // The state the file holds; none once that state is no longer in the history.
    std::optional<std::size_t> saved_;
};

// Where a dragged marker stops: inside [0, durationSeconds]. A number typed in
// is not clamped; Engine refuses it when the file is saved (gyo.anim_events
// Design Invariant 4: refuse, do not repair).
[[nodiscard]] double DragSeconds(double seconds, double durationSeconds) noexcept;

} // namespace Gyo::Tools::ModelViewer
