#include "object_connect/input/PointerWrap.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace object_connect::input {
namespace {

[[nodiscard]] Vec2 Extent(const AxisAlignedBox& bounds) noexcept {
    return {bounds.maximum.x - bounds.minimum.x, bounds.maximum.y - bounds.minimum.y};
}

[[nodiscard]] Vec2 ClientToPlayfieldDelta(const Vec2 delta, const AxisAlignedBox& bounds,
                                          const Vec2 clientSize) noexcept {
    const Vec2 extent = Extent(bounds);
    return {delta.x * extent.x / clientSize.x, delta.y * extent.y / clientSize.y};
}

[[nodiscard]] std::optional<float> MakeUnwrappedCoordinate(const double imageCoordinate, const std::int64_t winding,
                                                           const double extent) noexcept {
    const double value = imageCoordinate + static_cast<double>(winding) * extent;
    if (!std::isfinite(value) || std::abs(value) > static_cast<double>((std::numeric_limits<float>::max)())) {
        return std::nullopt;
    }
    const auto converted = static_cast<float>(value);
    return std::isfinite(converted) ? std::optional<float>{converted} : std::nullopt;
}

// The client pixel a playfield place maps to, clamped into the client (the
// original's MoveCursorToLogicalPosition without the Win32 call).
[[nodiscard]] Vec2 PlayfieldToClientPixel(const Vec2 position, const AxisAlignedBox& bounds,
                                          const Vec2 clientSize) noexcept {
    const Vec2 extent = Extent(bounds);
    const long maximumX = static_cast<long>(clientSize.x) - 1;
    const long maximumY = static_cast<long>(clientSize.y) - 1;
    const long x = std::clamp(std::lround((position.x - bounds.minimum.x) * clientSize.x / extent.x), 0L, maximumX);
    const long y = std::clamp(std::lround((position.y - bounds.minimum.y) * clientSize.y / extent.y), 0L, maximumY);
    return {static_cast<float>(x), static_cast<float>(y)};
}

} // namespace

bool CanMapPointer(const AxisAlignedBox& playfield, const Vec2 clientSize) noexcept {
    return IsValidAxisAlignedBox(playfield) && playfield.minimum.x < playfield.maximum.x &&
           playfield.minimum.y < playfield.maximum.y && std::isfinite(clientSize.x) && std::isfinite(clientSize.y) &&
           clientSize.x > 0.0f && clientSize.y > 0.0f;
}

Vec2 ClientToPlayfield(const Vec2 client, const AxisAlignedBox& playfield, const Vec2 clientSize) noexcept {
    const Vec2 extent = Extent(playfield);
    return {playfield.minimum.x + client.x * extent.x / clientSize.x,
            playfield.minimum.y + client.y * extent.y / clientSize.y};
}

void PointerWrap::Reset() noexcept {
    tracking_ = false;
    unwrapped_ = {};
    previousClient_ = {};
    winding_ = {};
    suppressRelativeX_ = false;
    suppressRelativeY_ = false;
}

PointerWrapResult PointerWrap::Sample(const PointerWrapSample& sample, const AxisAlignedBox& playfield) noexcept {
    PointerWrapResult result;
    if (!CanMapPointer(playfield, sample.clientSize)) {
        result.position = sample.clientPosition;
        Reset();
        return result;
    }
    const Vec2 client = sample.clientPosition;
    const Vec2 clientSize = sample.clientSize;
    const Vec2 pointer = ClientToPlayfield(client, playfield, clientSize);
    result.position = pointer;

    Vec2 clientDelta{};
    if (!tracking_) {
        tracking_ = true;
        unwrapped_ = pointer;
        previousClient_ = client;
        winding_ = {};
    } else {
        clientDelta = client - previousClient_;
        unwrapped_ += ClientToPlayfieldDelta(clientDelta, playfield, clientSize);
        previousClient_ = client;
    }

    // A warp can come back as one relative sample although the absolute
    // baseline already starts at the destination: ignore the relative motion
    // for exactly one sample after a warp, then take it again so a later real
    // crossing of the same edge is not lost.
    const bool suppressX = suppressRelativeX_;
    const bool suppressY = suppressRelativeY_;
    suppressRelativeX_ = false;
    suppressRelativeY_ = false;

    CanonicalWrappedPoint canonical = CanonicalizeWrappedPoint(unwrapped_, playfield);
    if (!canonical.valid) {
        Reset();
        return result;
    }

    const Vec2 extent = Extent(playfield);
    const float maximumClientX = clientSize.x - 1.0f;
    const float maximumClientY = clientSize.y - 1.0f;
    const float playfieldPixelX = extent.x / clientSize.x;
    const float playfieldPixelY = extent.y / clientSize.y;
    const Vec2 relative = sample.relativeMotion;
    const bool movingLeft = clientDelta.x < 0.0f || (clientDelta.x == 0.0f && !suppressX && relative.x < 0.0f);
    const bool movingRight = clientDelta.x > 0.0f || (clientDelta.x == 0.0f && !suppressX && relative.x > 0.0f);
    const bool movingUp = clientDelta.y < 0.0f || (clientDelta.y == 0.0f && !suppressY && relative.y < 0.0f);
    const bool movingDown = clientDelta.y > 0.0f || (clientDelta.y == 0.0f && !suppressY && relative.y > 0.0f);
    bool boundaryCoordinateValid = true;

    // Cross at the last pixel inside only while the motion on that axis points
    // out of the client. Where the cursor is stopped at an edge, the relative
    // motion gives the direction without moving the pointer.
    if (clientSize.x > 1.0f && client.x <= 0.0f && movingLeft && canonical.winding.x == winding_.x) {
        const std::optional<float> crossed = MakeUnwrappedCoordinate(
            static_cast<double>(playfield.minimum.x) - playfieldPixelX, winding_.x, extent.x);
        boundaryCoordinateValid = crossed.has_value();
        if (crossed) {
            unwrapped_.x = *crossed;
        }
    } else if (clientSize.x > 1.0f && client.x >= maximumClientX && movingRight &&
               canonical.winding.x == winding_.x) {
        const std::optional<float> crossed = MakeUnwrappedCoordinate(playfield.maximum.x, winding_.x, extent.x);
        boundaryCoordinateValid = crossed.has_value();
        if (crossed) {
            unwrapped_.x = *crossed;
        }
    }
    if (clientSize.y > 1.0f && client.y <= 0.0f && movingUp && canonical.winding.y == winding_.y) {
        const std::optional<float> crossed = MakeUnwrappedCoordinate(
            static_cast<double>(playfield.minimum.y) - playfieldPixelY, winding_.y, extent.y);
        boundaryCoordinateValid = boundaryCoordinateValid && crossed.has_value();
        if (crossed) {
            unwrapped_.y = *crossed;
        }
    } else if (clientSize.y > 1.0f && client.y >= maximumClientY && movingDown &&
               canonical.winding.y == winding_.y) {
        const std::optional<float> crossed = MakeUnwrappedCoordinate(playfield.maximum.y, winding_.y, extent.y);
        boundaryCoordinateValid = boundaryCoordinateValid && crossed.has_value();
        if (crossed) {
            unwrapped_.y = *crossed;
        }
    }
    if (!boundaryCoordinateValid) {
        Reset();
        return result;
    }

    canonical = CanonicalizeWrappedPoint(unwrapped_, playfield);
    if (!canonical.valid) {
        Reset();
        return result;
    }

    result.position = canonical.position;
    result.unwrappedPosition = unwrapped_;
    if (canonical.winding != winding_) {
        // The window applies the warp before the next frame's events, so the
        // next sample's baseline is the destination and the warp itself never
        // becomes movement.
        const Vec2 target = PlayfieldToClientPixel(canonical.position, playfield, clientSize);
        result.warpTo = target;
        previousClient_ = target;
        if (canonical.winding.x != winding_.x) {
            suppressRelativeX_ = true;
        }
        if (canonical.winding.y != winding_.y) {
            suppressRelativeY_ = true;
        }
        winding_ = canonical.winding;
    }
    return result;
}

} // namespace object_connect::input
