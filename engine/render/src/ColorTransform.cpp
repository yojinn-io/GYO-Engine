#include "render/ColorTransform.hpp"

#include "engine/math/scalar/ColorSpace.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <algorithm>
#include <cmath>

namespace Engine::Render {
namespace {

[[nodiscard]] float TransformComponent(
    const float linear,
    const float exposureScale,
    const float inverseGamma) noexcept {
    const float exposed = Math::Clamp((std::max)(linear, 0.0F) * exposureScale, 0.0F, 1.0F);
    return std::pow(exposed, inverseGamma);
}

} // namespace

Color DecodeSrgbColor(const Color encoded) noexcept {
    return {
        Math::DecodeSrgb(encoded.red),
        Math::DecodeSrgb(encoded.green),
        Math::DecodeSrgb(encoded.blue),
        encoded.alpha,
    };
}

Color EncodeSrgbColor(const Color linear) noexcept {
    return {
        Math::EncodeSrgb(linear.red),
        Math::EncodeSrgb(linear.green),
        Math::EncodeSrgb(linear.blue),
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
