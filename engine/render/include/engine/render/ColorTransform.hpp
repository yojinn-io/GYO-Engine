#pragma once

#include "engine/render/RenderTypes.hpp"

namespace Engine::Render {

inline constexpr float kMinimumSceneExposureEv = -8.0F;
inline constexpr float kMaximumSceneExposureEv = 8.0F;
inline constexpr float kMinimumSceneGammaAdjustment = 0.25F;
inline constexpr float kMaximumSceneGammaAdjustment = 4.0F;

// RGB follows the standard sRGB transfer (Math::DecodeSrgb/EncodeSrgb, inputs
// clamped to [0, 1]) while straight alpha is preserved.
[[nodiscard]] Color DecodeSrgbColor(Color encoded) noexcept;
[[nodiscard]] Color EncodeSrgbColor(Color linear) noexcept;

[[nodiscard]] bool IsValidSceneColorTransform(
    SceneColorTransform transform) noexcept;

// CPU reference for the SDL_GPU scene post shader: exposure is applied in
// linear space, clamped to SDR, then the relative gamma adjustment is applied.
// Straight alpha is not part of the display adjustment and is preserved.
[[nodiscard]] Color ApplySceneColorTransform(
    Color linear,
    SceneColorTransform transform) noexcept;

} // namespace Engine::Render
