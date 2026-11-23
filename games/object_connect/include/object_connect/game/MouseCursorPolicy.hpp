#pragma once

#include "object_connect/game/GameFlow.hpp"

#include <cstdint>

namespace object_connect {

// Which cursor image the game shows, and when it replaces the system cursor.
// Moved unchanged out of the original MouseCursorRenderer.hpp: the rule is game
// logic; drawing the sprite and hiding the system cursor wait for the window and
// render batches.
enum class MouseCursorIcon : std::uint8_t {
    Normal,
    Interactive,
    Holding,
};

[[nodiscard]] constexpr MouseCursorIcon ResolveMouseCursorIcon(
    const GameScreen screen, const bool isDragging,
    const bool pointerOverAction) noexcept {
    if (screen == GameScreen::Playing) {
        return isDragging ? MouseCursorIcon::Holding
                          : MouseCursorIcon::Normal;
    }
    return pointerOverAction ? MouseCursorIcon::Interactive
                             : MouseCursorIcon::Normal;
}

[[nodiscard]] constexpr bool ShouldUseCustomMouseCursor(
    const bool windowFocused, const bool insideClient) noexcept {
    return windowFocused && insideClient;
}

} // namespace object_connect
