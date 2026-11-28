// High dynamic range and tonemapping (Tonemapping.hpp). The curves are Bevy's (bevy_core_pipeline/src/tonemapping.wesl
// at 6a911653f, line numbers below) and the same as shaders/Tonemapping.hlsli.
#include "engine/render/Tonemapping.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "engine/render/ShaderAbi.hpp"

namespace GYO::Engine::Render {

namespace {

using Math::Vec3;

// Rec. 709 luminance (tonemapping.wesl:236-238).
float Luminance(const Vec3 c) noexcept { return 0.2126F * c.x + 0.7152F * c.y + 0.0722F * c.z; }

Vec3 Mix(const Vec3 a, const Vec3 b, const float t) noexcept { return Math::Lerp(a, b, t); }

Vec3 Reinhard(const Vec3 c) noexcept { return {c.x / (1.0F + c.x), c.y / (1.0F + c.y), c.z / (1.0F + c.z)}; }

// tonemapping.wesl:240-249. Black stays black (Bevy's divides 0 by 0 there: a NaN that the GPU's write turns into 0
// or not).
Vec3 ReinhardLuminance(const Vec3 c) noexcept {
    const float before = Luminance(c);
    if (before <= 0.0F) return Vec3{};
    const float after = before / (1.0F + before);
    return c * (after / before);
}

// tonemapping.wesl:106-140: row vector times the matrices of columns given there.
Vec3 AcesFitted(const Vec3 c) noexcept {
    const Vec3 rrt{Math::Dot(c, {0.59719F, 0.35458F, 0.04823F}), Math::Dot(c, {0.07600F, 0.90834F, 0.01566F}),
                   Math::Dot(c, {0.02840F, 0.13383F, 0.83777F})};
    const auto fit = [](const float v) { return (v * (v + 0.0245786F) - 0.000090537F) / (v * (0.983729F * v + 0.4329510F) + 0.238081F); };
    const Vec3 fitted{fit(rrt.x), fit(rrt.y), fit(rrt.z)};
    const Vec3 odt{Math::Dot(fitted, {1.60475F, -0.53108F, -0.07367F}), Math::Dot(fitted, {-0.10208F, 1.10813F, -0.00605F}),
                   Math::Dot(fitted, {-0.00327F, -0.07276F, 1.07602F})};
    return {std::clamp(odt.x, 0.0F, 1.0F), std::clamp(odt.y, 0.0F, 1.0F), std::clamp(odt.z, 0.0F, 1.0F)};
}

// tonemapping.wesl:48-56 (the branch in use).
float BoringCurve(const float v) noexcept { return 1.0F - std::exp(-v); }

// tonemapping.wesl:39-80.
Vec3 SomewhatBoringDisplayTransform(const Vec3 c) noexcept {
    const Vec3 ycbcr{Math::Dot(c, {0.2126F, 0.7152F, 0.0722F}), Math::Dot(c, {-0.1146F, -0.3854F, 0.5F}),
                     Math::Dot(c, {0.5F, -0.4542F, -0.0458F})};
    const float bt = BoringCurve(std::sqrt(ycbcr.y * ycbcr.y + ycbcr.z * ycbcr.z) * 2.4F);
    float desat = std::max((bt - 0.7F) * 0.8F, 0.0F);
    desat *= desat;
    const Vec3 desatColour = Mix(c, {ycbcr.x, ycbcr.x, ycbcr.x}, desat);
    const float tmLuma = BoringCurve(ycbcr.x);
    const Vec3 tm0 = c * std::max(0.0F, tmLuma / std::max(1e-5F, Luminance(c)));
    const Vec3 tm1{BoringCurve(desatColour.x), BoringCurve(desatColour.y), BoringCurve(desatColour.z)};
    return Mix(tm0, tm1, bt * bt) * 0.97F;
}

// tonemapping.wesl:258-301.
Vec3 KhronosPbrNeutral(const Vec3 c) noexcept {
    constexpr float kStartCompression = 0.8F - 0.04F;
    constexpr float kDesaturation = 0.15F;
    const float x = std::min(c.x, std::min(c.y, c.z));
    const float offset = x < 0.08F ? x - 6.25F * x * x : 0.04F;
    const Vec3 offsetColour{c.x - offset, c.y - offset, c.z - offset};
    const float peak = std::max(offsetColour.x, std::max(offsetColour.y, offsetColour.z));
    if (peak < kStartCompression) return offsetColour;
    constexpr float kD = 1.0F - kStartCompression;
    const float newPeak = 1.0F - kD * kD / (peak + kD - kStartCompression);
    const Vec3 colour = offsetColour * (newPeak / peak);
    const float g = 1.0F - 1.0F / (kDesaturation * (peak - newPeak) + 1.0F);
    return Mix(colour, {newPeak, newPeak, newPeak}, g);
}

std::uint8_t ToByte(const float unit) noexcept { return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0F, 1.0F) * 255.0F)); }

} // namespace

float Exposure::Scale() const noexcept { return std::exp2(-ev100) / 1.2F; }

float SrgbToLinear(const float encoded) noexcept {
    return encoded <= 0.04045F ? encoded / 12.92F : std::pow((encoded + 0.055F) / 1.055F, 2.4F);
}

float LinearToSrgb(const float linear) noexcept {
    const float c = std::clamp(linear, 0.0F, 1.0F);
    return c <= 0.0031308F ? c * 12.92F : 1.055F * std::pow(c, 1.0F / 2.4F) - 0.055F;
}

Math::Vec3 Tonemap(const Tonemapping::Method method, const Math::Vec3 linear) noexcept {
    const Vec3 c = Math::Max(linear, Vec3{});
    switch (method) {
    case Tonemapping::Method::None: return c;
    case Tonemapping::Method::Reinhard: return Reinhard(c);
    case Tonemapping::Method::ReinhardLuminance: return ReinhardLuminance(c);
    case Tonemapping::Method::AcesFitted: return AcesFitted(c);
    case Tonemapping::Method::SomewhatBoringDisplayTransform: return SomewhatBoringDisplayTransform(c);
    case Tonemapping::Method::KhronosPbrNeutral: return KhronosPbrNeutral(c);
    }
    return c;
}

Color8 TonemapToBytes(const Tonemapping::Method method, const Math::Vec3 linear, const float alpha) noexcept {
    const Vec3 mapped = Tonemap(method, linear);
    return Color8{ToByte(LinearToSrgb(mapped.x)), ToByte(LinearToSrgb(mapped.y)), ToByte(LinearToSrgb(mapped.z)), ToByte(alpha)};
}

PassFullscreen TonemappingPass(const TextureId picture, std::optional<TextureId> output, std::optional<PixelRect> viewport,
                               const Tonemapping::Method method) {
    PassFullscreen pass;
    pass.target = std::move(output);
    pass.viewport = std::move(viewport);
    pass.shader = std::string(ShaderAbi::ShaderName(ShaderAbi::kTonemappingFragmentSource));
    pass.inputs = {picture};
    const ShaderAbi::TonemappingParameters parameters = ShaderAbi::ToParameters(method);
    pass.parameters.resize(sizeof(parameters));
    std::memcpy(pass.parameters.data(), &parameters, sizeof(parameters));
    return pass;
}

const TextureId* HdrTargets::Find(const std::optional<TextureId>& output) const noexcept {
    for (const Entry& entry : entries) {
        if (entry.output == output) return &entry.picture;
    }
    return nullptr;
}

void SyncHdrTargets(const std::vector<std::optional<TextureId>>& outputs, RenderTextures& textures, HdrTargets& pictures) {
    std::vector<HdrTargets::Entry> next;
    for (const std::optional<TextureId>& output : outputs) {
        if (std::any_of(next.begin(), next.end(), [&](const HdrTargets::Entry& e) { return e.output == output; })) continue;
        const auto kept = std::find_if(pictures.entries.begin(), pictures.entries.end(),
                                       [&](const HdrTargets::Entry& e) { return e.output == output; });
        if (kept != pictures.entries.end()) {
            next.push_back(*kept);
            pictures.entries.erase(kept);
            continue;
        }
        // Linear light, as large as the camera target; sampled at its own pixels (the nearest texel).
        RenderTargetDesc desc;
        desc.size = FollowTarget{output};
        desc.format = RenderTargetFormat::Rgba16Float;
        desc.filter = TextureFilter::Nearest;
        next.push_back(HdrTargets::Entry{output, textures.CreateTarget(desc)});
    }
    for (const HdrTargets::Entry& gone : pictures.entries) textures.Release(gone.picture);
    pictures.entries = std::move(next);
}

} // namespace GYO::Engine::Render
