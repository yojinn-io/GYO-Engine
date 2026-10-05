#pragma once

#include "engine/input/PhysicalInputFrame.hpp"

#include <span>

namespace fps::pvp {

// In-world pointer capture policy: when the client takes the gameplay pointer,
// when it gives it up, and which click is a shot. The product owns this policy;
// Engine input only reports the window's events.
struct PointerCaptureState final {
    bool inputCaptured{};
    bool leftButtonDown{};
    bool pendingShotEdge{};
    bool pendingReloadEdge{};
    // Per frame: the caller clears these before a frame's events.
    bool pointerAcquiredThisFrame{};
    bool windowInteraction{};
};

struct PointerCaptureResult final {
    unsigned releases{};               // times the pointer was released
    unsigned releasesWhileCaptured{};  // of those, while it was captured
};

// Applies one frame's events of this window in arrival order. focusedAtStart is
// the window focus before the first event; focus events update it as they come.
// A release clears the capture, both pending edges and the held left button;
// the caller then turns relative mouse mode off and clears the jump request.
PointerCaptureResult ApplyPointerCaptureEvents(PointerCaptureState& state,
    std::span<const Engine::Input::InputEvent> events, bool focusedAtStart) noexcept;

} // namespace fps::pvp
