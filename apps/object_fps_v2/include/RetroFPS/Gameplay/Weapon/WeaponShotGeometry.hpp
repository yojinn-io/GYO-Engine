#pragma once

#include "RetroFPS/Data/GameData.hpp"
#include "RetroFPS/Math/Vector.hpp"

#include <cmath>
#include <numbers>
#include <unordered_map>

namespace fps {

// Authored muzzle in the dedicated viewmodel camera's coordinates: +X right,
// +Y up, +Z forward. Campaign loading supplies this numeric calibration so the
// simulation does not need models, animation, or renderer resources.
struct WeaponShotGeometry final {
    Float3 muzzleViewCameraPosition{};
    float viewModelVerticalFovRadians{};
};

using WeaponShotGeometryMap =
    std::unordered_map<WeaponDefinitionId, WeaponShotGeometry>;

[[nodiscard]] inline bool IsValidWeaponVerticalFov(const float radians) noexcept {
    return std::isfinite(radians) && radians > 0.0F &&
           radians < std::numbers::pi_v<float>;
}

// Match the viewmodel muzzle's screen position in the world camera while
// retaining its authored depth. Both FOVs must have passed content validation.
[[nodiscard]] inline Float3 ResolveWeaponMuzzleCameraPosition(
    const WeaponShotGeometry& geometry,
    const float worldVerticalFovRadians) noexcept {
    const float scale = std::tan(worldVerticalFovRadians * 0.5F) /
                        std::tan(geometry.viewModelVerticalFovRadians * 0.5F);
    return {
        geometry.muzzleViewCameraPosition.x * scale,
        geometry.muzzleViewCameraPosition.y * scale,
        geometry.muzzleViewCameraPosition.z,
    };
}

} // namespace fps
