#pragma once

// The tests' CPU reference of the tonemapping curves (render-pipeline declaration section 16): the formulas of Bevy's
// bevy_core_pipeline/src/tonemapping.wesl at 6a911653f (line numbers below), the same as render's shaders/Tonemapping.hlsli.
// render.data compares these with values of Bevy's formulas worked out apart from them (TonemappingTests.cpp);
// sdl_gpu.hdr compares what the GPU drew with these (HdrTests.cpp). Header only, test code: nothing of the Engine calls
// it.

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "engine/math/linear/Vec3.hpp"
#include "engine/math/scalar/ColorSpace.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/RenderCommands.hpp"

namespace GYO::Test::Tonemapping {

namespace detail {

using Engine::Math::Vec3;

// Rec. 709 luminance (tonemapping.wesl:236-238).
inline float Luminance(const Vec3 c) noexcept { return 0.2126F * c.x + 0.7152F * c.y + 0.0722F * c.z; }

inline Vec3 Reinhard(const Vec3 c) noexcept { return {c.x / (1.0F + c.x), c.y / (1.0F + c.y), c.z / (1.0F + c.z)}; }

// tonemapping.wesl:240-249; black stays black (Bevy's divides 0 by 0 there).
inline Vec3 ReinhardLuminance(const Vec3 c) noexcept {
    const float before = Luminance(c);
    if (before <= 0.0F) return Vec3{};
    return c * ((before / (1.0F + before)) / before);
}

// tonemapping.wesl:106-140: row vector times the matrices of the columns given there.
inline Vec3 AcesFitted(const Vec3 c) noexcept {
    using Engine::Math::Dot;
    const Vec3 rrt{Dot(c, {0.59719F, 0.35458F, 0.04823F}), Dot(c, {0.07600F, 0.90834F, 0.01566F}), Dot(c, {0.02840F, 0.13383F, 0.83777F})};
    const auto fit = [](const float v) { return (v * (v + 0.0245786F) - 0.000090537F) / (v * (0.983729F * v + 0.4329510F) + 0.238081F); };
    const Vec3 fitted{fit(rrt.x), fit(rrt.y), fit(rrt.z)};
    const Vec3 odt{Dot(fitted, {1.60475F, -0.53108F, -0.07367F}), Dot(fitted, {-0.10208F, 1.10813F, -0.00605F}),
                   Dot(fitted, {-0.00327F, -0.07276F, 1.07602F})};
    return {std::clamp(odt.x, 0.0F, 1.0F), std::clamp(odt.y, 0.0F, 1.0F), std::clamp(odt.z, 0.0F, 1.0F)};
}

// tonemapping.wesl:48-56 (the branch in use) and 39-80.
inline float BoringCurve(const float v) noexcept { return 1.0F - std::exp(-v); }

inline Vec3 SomewhatBoringDisplayTransform(const Vec3 c) noexcept {
    using Engine::Math::Dot;
    using Engine::Math::Lerp;
    const Vec3 ycbcr{Dot(c, {0.2126F, 0.7152F, 0.0722F}), Dot(c, {-0.1146F, -0.3854F, 0.5F}), Dot(c, {0.5F, -0.4542F, -0.0458F})};
    const float bt = BoringCurve(std::sqrt(ycbcr.y * ycbcr.y + ycbcr.z * ycbcr.z) * 2.4F);
    float desat = std::max((bt - 0.7F) * 0.8F, 0.0F);
    desat *= desat;
    const Vec3 desatColour = Lerp(c, {ycbcr.x, ycbcr.x, ycbcr.x}, desat);
    const Vec3 tm0 = c * std::max(0.0F, BoringCurve(ycbcr.x) / std::max(1e-5F, Luminance(c)));
    const Vec3 tm1{BoringCurve(desatColour.x), BoringCurve(desatColour.y), BoringCurve(desatColour.z)};
    return Lerp(tm0, tm1, bt * bt) * 0.97F;
}

// tonemapping.wesl:258-301.
inline Vec3 KhronosPbrNeutral(const Vec3 c) noexcept {
    constexpr float kStartCompression = 0.8F - 0.04F;
    constexpr float kDesaturation = 0.15F;
    const float x = std::min(c.x, std::min(c.y, c.z));
    const float offset = x < 0.08F ? x - 6.25F * x * x : 0.04F;
    const Vec3 offsetColour{c.x - offset, c.y - offset, c.z - offset};
    const float peak = std::max(offsetColour.x, std::max(offsetColour.y, offsetColour.z));
    if (peak < kStartCompression) return offsetColour;
    constexpr float kD = 1.0F - kStartCompression;
    const float newPeak = 1.0F - kD * kD / (peak + kD - kStartCompression);
    const float g = 1.0F - 1.0F / (kDesaturation * (peak - newPeak) + 1.0F);
    return Engine::Math::Lerp(offsetColour * (newPeak / peak), {newPeak, newPeak, newPeak}, g);
}

inline std::uint8_t ToByte(const float unit) noexcept { return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0F, 1.0F) * 255.0F)); }

} // namespace detail

using Method = Engine::Render::Tonemapping::Method;

// The method's curve on linear light, after Bevy's tone_mapping (tonemapping.wesl:373-428) with no colour grading:
// negative channels are taken as 0 first. Not clamped.
inline Engine::Math::Vec3 Tonemap(const Method method, const Engine::Math::Vec3 linear) noexcept {
    const Engine::Math::Vec3 c = Engine::Math::Max(linear, Engine::Math::Vec3{});
    switch (method) {
    case Method::None: return c;
    case Method::Reinhard: return detail::Reinhard(c);
    case Method::ReinhardLuminance: return detail::ReinhardLuminance(c);
    case Method::AcesFitted: return detail::AcesFitted(c);
    case Method::SomewhatBoringDisplayTransform: return detail::SomewhatBoringDisplayTransform(c);
    case Method::KhronosPbrNeutral: return detail::KhronosPbrNeutral(c);
    }
    return c;
}

// What a tonemapping pass writes for a pixel of the picture: the curve, each channel sRGB-encoded (Math's EncodeSrgb)
// to the nearest byte; alpha (0 to 1) to the nearest byte as it is.
inline Engine::Render::Color8 TonemapToBytes(const Method method, const Engine::Math::Vec3 linear, const float alpha) noexcept {
    using Engine::Math::EncodeSrgb;
    const Engine::Math::Vec3 m = Tonemap(method, linear);
    return {detail::ToByte(EncodeSrgb(m.x)), detail::ToByte(EncodeSrgb(m.y)), detail::ToByte(EncodeSrgb(m.z)), detail::ToByte(alpha)};
}

} // namespace GYO::Test::Tonemapping
