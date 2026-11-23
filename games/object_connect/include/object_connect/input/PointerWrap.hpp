#pragma once

#include "object_connect/geometry/Geometry2D.hpp"
#include "object_connect/math/Vec2.hpp"

#include <optional>

// The pointer on wrapEdges levels: while a vessel is dragged, pushing the
// cursor out of one edge brings it back at the opposite edge, and the drag
// goes on through the edge. Moved out of the original InputSystem
// (Object_Connect ca4482c, src/ObjectConnect/Input/InputSystem.cpp:150-330)
// without the window: positions come in, a warp target goes out, and the
// caller asks the window to move the cursor there.
//
// "Client" is the window's drawable area in the window's own units (the
// original's client pixels, GYO's logical pixels), origin at its top-left
// corner; the playfield is the game's coordinates.

namespace object_connect::input {

// A place in the client mapped to the playfield (the original's ToLogicalPosition).
[[nodiscard]] Vec2 ClientToPlayfield(Vec2 client, const AxisAlignedBox& playfield, Vec2 clientSize) noexcept;

// The bounds can map client places (a positive extent on both axes) and the client has a size.
[[nodiscard]] bool CanMapPointer(const AxisAlignedBox& playfield, Vec2 clientSize) noexcept;

struct PointerWrapSample final {
    // Where the cursor is in the client this frame (outside it while a button holds the capture).
    Vec2 clientPosition{};
    Vec2 clientSize{};
    // The device's motion this frame; only its sign on each axis is read, to
    // tell a push against an edge the cursor cannot pass (the original's
    // DirectInput relative motion).
    Vec2 relativeMotion{};
};

struct PointerWrapResult final {
    // The pointer in the playfield, wrapped into it.
    Vec2 position{};
    // The pointer without wrapping, for the board's path through the edges.
    std::optional<Vec2> unwrappedPosition;
    // Move the system cursor here (client units): it crossed an edge.
    std::optional<Vec2> warpTo;
};

class PointerWrap final {
public:
    // One frame while wrapping. Without a mapping or with a value out of range
    // it resets and gives the plain mapped place.
    [[nodiscard]] PointerWrapResult Sample(const PointerWrapSample& sample, const AxisAlignedBox& playfield) noexcept;
    // The drag ended or the level changed: the next Sample starts again.
    void Reset() noexcept;

private:
    bool tracking_ = false;
    Vec2 unwrapped_{};
    Vec2 previousClient_{};
    WrapWinding winding_{};
    bool suppressRelativeX_ = false;
    bool suppressRelativeY_ = false;
};

} // namespace object_connect::input
