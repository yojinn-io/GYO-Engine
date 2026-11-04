#include "gyo/model_viewer/EventHistory.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cmath>
#include <utility>

namespace Gyo::Tools::ModelViewer {

EventHistory::EventHistory(Engine::Model::AnimationEventSet events, const bool saved)
    : document_(events), states_{std::move(events)} {
    if (saved) saved_ = 0;
}

void EventHistory::Record() {
    states_.resize(current_ + 1);
    if (saved_ && *saved_ > current_) saved_.reset();
    states_.push_back(document_.Set());
    ++current_;
}

std::size_t EventHistory::Add(const std::string_view clip, Engine::Model::AnimationEvent event) {
    const std::size_t index = document_.Add(clip, std::move(event));
    Record();
    return index;
}

std::size_t EventHistory::Move(const std::string_view clip, const std::size_t index, const double seconds) {
    const auto* track = document_.Track(clip);
    if (track != nullptr && index < track->events.size() &&
        std::bit_cast<std::uint64_t>(track->events[index].seconds) == std::bit_cast<std::uint64_t>(seconds)) {
        return index;
    }
    const std::size_t moved = document_.Move(clip, index, seconds);
    Record();
    return moved;
}

void EventHistory::Rename(const std::string_view clip, const std::size_t index, std::string name) {
    const auto* track = document_.Track(clip);
    if (track != nullptr && index < track->events.size() && track->events[index].name == name) return;
    document_.Rename(clip, index, std::move(name));
    Record();
}

void EventHistory::Remove(const std::string_view clip, const std::size_t index) {
    document_.Remove(clip, index);
    Record();
}

bool EventHistory::Undo() {
    if (!CanUndo()) return false;
    --current_;
    document_ = EventDocument(states_[current_]);
    return true;
}

bool EventHistory::Redo() {
    if (!CanRedo()) return false;
    ++current_;
    document_ = EventDocument(states_[current_]);
    return true;
}

double DragSeconds(const double seconds, const double durationSeconds) noexcept {
    if (!std::isfinite(seconds)) return 0.0;
    return std::clamp(seconds, 0.0, std::max(durationSeconds, 0.0));
}

} // namespace Gyo::Tools::ModelViewer
