#include "RetroFPS/Pvp/PointerCapture.hpp"

namespace fps::pvp {

PointerCaptureResult ApplyPointerCaptureEvents(PointerCaptureState& state,
    const std::span<const Engine::Input::InputEvent> events, const bool focusedAtStart) noexcept {
    using Engine::Input::InputEventKind;
    using Engine::Input::Key;
    using Engine::Input::MouseButton;

    PointerCaptureResult result;
    const auto release = [&] {
        ++result.releases;
        if (state.inputCaptured) ++result.releasesWhileCaptured;
        state.inputCaptured = false;
        state.pendingShotEdge = state.pendingReloadEdge = false;
        state.leftButtonDown = false;
    };

    bool focused = focusedAtStart;
    for (const auto& event : events) {
        switch (event.kind) {
        case InputEventKind::WindowFocusGained:
            focused = true;
            break;
        case InputEventKind::WindowFocusLost:
            focused = false;
            // Losing focus, moving, resizing or minimizing the window must not
            // keep the gameplay mouse lock.
            state.windowInteraction = true;
            release();
            break;
        case InputEventKind::WindowMoved:
        case InputEventKind::WindowResized:
        case InputEventKind::WindowMinimized:
            state.windowInteraction = true;
            release();
            break;
        case InputEventKind::KeyPressed:
            if (event.key == Key::Tab && focused && !state.windowInteraction) {
                if (state.inputCaptured) {
                    state.windowInteraction = true;
                    release();
                } else {
                    state.inputCaptured = true;
                    state.pointerAcquiredThisFrame = true;
                }
            }
            break;
        case InputEventKind::MouseButtonPressed:
            if (event.button == MouseButton::Left && focused && !state.windowInteraction && event.insideWindow) {
                const bool rising = !state.leftButtonDown;
                state.leftButtonDown = true;
                if (!state.inputCaptured) {
                    state.inputCaptured = true;
                    state.pointerAcquiredThisFrame = true;
                } else if (rising && !state.pointerAcquiredThisFrame) {
                    state.pendingShotEdge = true;
                }
            }
            break;
        case InputEventKind::MouseButtonReleased:
            if (event.button == MouseButton::Left) state.leftButtonDown = false;
            break;
        default:
            break;
        }
    }
    return result;
}

} // namespace fps::pvp
