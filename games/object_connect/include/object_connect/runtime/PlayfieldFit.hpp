#pragma once

#include "object_connect/math/Vec2.hpp"

#include "engine/math/linear/VecInt.hpp"
#include "engine/render/RenderCommands.hpp"

#include <optional>

// Where the 1280 x 720 playfield sits in the window: the largest rectangle of its aspect centred in the window, the
// rest left black (camera's Letterbox through camera::Compute, batch R2b). The window is not resizable (G6), so the
// rectangle is normally the whole window; the bars appear when the window's drawable area has another shape (a
// screen too small for it, a scale factor that rounds). The picture is drawn in it (Pass2D::viewport), and the
// pointer is mapped through it (and back, for a warp).

namespace object_connect::runtime {

struct PlayfieldFit final {
    // On the target, in physical pixels (Pass2D::viewport).
    GYO::Engine::Render::PixelRect physical;
    // In the window's logical pixels (the input's positions).
    Vec2 logicalOrigin{};
    Vec2 logicalSize{};
};

// nullopt while the window's size is not known or has no area.
[[nodiscard]] std::optional<PlayfieldFit> FitPlayfield(GYO::Engine::Math::Vec2i physicalSize, float scaleFactor);

} // namespace object_connect::runtime
