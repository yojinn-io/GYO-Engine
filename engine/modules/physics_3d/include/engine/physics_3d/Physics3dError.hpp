#pragma once

// The errors of the physics_3d Module (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, sections 4.3
// to 4.5). They say what happened; what to do about it is the caller's.

#include <cstdint>

#include "engine/base/Error.hpp"

namespace GYO::Engine::Physics3d {

enum class Physics3dErrorCode : std::uint8_t {
    // A box given to BoxSet::Build is not finite or has minimum > maximum on an axis (detail: the index).
    InvalidBox = 1,
    // More boxes than a 32-bit index can name.
    TooManyBoxes = 2,
    // A collider's Transform has a scale that is not exactly (1, 1, 1) (detail: the entity). Scaled colliders are
    // not supported in this batch; the scale is never ignored.
    UnsupportedScale = 3,
    // The query is not implemented for this pair of shapes (the table of section 4.3); no approximation is
    // returned.
    Unsupported = 4,
};

[[nodiscard]] constexpr const char* ToString(const Physics3dErrorCode code) noexcept {
    switch (code) {
    case Physics3dErrorCode::InvalidBox:       return "InvalidBox";
    case Physics3dErrorCode::TooManyBoxes:     return "TooManyBoxes";
    case Physics3dErrorCode::UnsupportedScale: return "UnsupportedScale";
    case Physics3dErrorCode::Unsupported:      return "Unsupported";
    }
    return "Unknown";
}

using Physics3dError = Base::Error<Physics3dErrorCode>;
static_assert(Base::CodedError<Physics3dError>);

} // namespace GYO::Engine::Physics3d
