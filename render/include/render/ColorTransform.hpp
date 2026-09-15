#pragma once

#include "render/RenderTypes.hpp"

namespace Engine::Render {

inline constexpr float kMinimumSceneExposureEv = -8.0F;
inline constexpr float kMaximumSceneExposureEv = 8.0F;
inline constexpr float kMinimumSceneGammaAdjustment = 0.25F;
inline constexpr float kMaximumSceneGammaAdjustment = 4.0F;

// Standard sRGB transfer functions for normalized components. Inputs are
// clamped to the representable [0, 1] display range.
[[nodiscard]] float DecodeSrgbComponent(float encoded) noexcept;
[[nodiscard]] float EncodeSrgbComponent(float linear) noexcept;

// RGB follows the standard sRGB transfer while straight alpha is preserved.
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
