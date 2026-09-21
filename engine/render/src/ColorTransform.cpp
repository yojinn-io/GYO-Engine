#include "render/ColorTransform.hpp"

#include <algorithm>
#include <cmath>

namespace Engine::Render {
namespace {

[[nodiscard]] float ClampUnit(const float value) noexcept {
    return std::clamp(value, 0.0F, 1.0F);
}

[[nodiscard]] float TransformComponent(
    const float linear,
    const float exposureScale,
    const float inverseGamma) noexcept {
    const float exposed = ClampUnit((std::max)(linear, 0.0F) * exposureScale);
    return std::pow(exposed, inverseGamma);
}

} // namespace

float DecodeSrgbComponent(const float encoded) noexcept {
    const float value = ClampUnit(encoded);
    return value <= 0.04045F
        ? value / 12.92F
        : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

float EncodeSrgbComponent(const float linear) noexcept {
    const float value = ClampUnit(linear);
    return value <= 0.0031308F
        ? value * 12.92F
        : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
}

Color DecodeSrgbColor(const Color encoded) noexcept {
    return {
        DecodeSrgbComponent(encoded.red),
        DecodeSrgbComponent(encoded.green),
        DecodeSrgbComponent(encoded.blue),
        encoded.alpha,
    };
}

Color EncodeSrgbColor(const Color linear) noexcept {
    return {
        EncodeSrgbComponent(linear.red),
        EncodeSrgbComponent(linear.green),
        EncodeSrgbComponent(linear.blue),
        linear.alpha,
    };
}

bool IsValidSceneColorTransform(const SceneColorTransform transform) noexcept {
    return std::isfinite(transform.exposureEv) &&
           std::isfinite(transform.gammaAdjustment) &&
           transform.exposureEv >= kMinimumSceneExposureEv &&
           transform.exposureEv <= kMaximumSceneExposureEv &&
           transform.gammaAdjustment >= kMinimumSceneGammaAdjustment &&
           transform.gammaAdjustment <= kMaximumSceneGammaAdjustment;
}

Color ApplySceneColorTransform(
    const Color linear,
    const SceneColorTransform transform) noexcept {
    if (!IsValidSceneColorTransform(transform)) {
        return linear;
    }
    const float exposureScale = std::exp2(transform.exposureEv);
    const float inverseGamma = 1.0F / transform.gammaAdjustment;
    return {
        TransformComponent(linear.red, exposureScale, inverseGamma),
        TransformComponent(linear.green, exposureScale, inverseGamma),
        TransformComponent(linear.blue, exposureScale, inverseGamma),
        linear.alpha,
    };
}

} // namespace Engine::Render
