#ifndef GYO_RENDER_TONEMAPPING_INCLUDED
#define GYO_RENDER_TONEMAPPING_INCLUDED
// The tonemapping curves (render-pipeline declaration section 16): Bevy's, bevy_core_pipeline/src/tonemapping.wesl at
// 6a911653f. The C++ side is the tests' reference (tests/common/render/TonemappingReference.hpp) and ShaderAbi.hpp
// (Design Invariant RD-4). The tonemapping pass includes it; so will a lit material that tonemaps in its own shader on
// a camera without Hdr (Bevy's TONEMAP_IN_SHADER, B4).

// Tonemapping::Method's values: one list for both sides (the path is from this directory, the include directory).
#include "../include/engine/render/TonemappingMethods.h"

// Rec. 709 luminance (tonemapping.wesl:236-238).
float TonemappingLuminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

// tonemapping.wesl:240-249; black stays black (Bevy's divides 0 by 0 there).
float3 TonemapReinhardLuminance(float3 c)
{
    const float before = TonemappingLuminance(c);
    if (before <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }
    const float after = before / (1.0f + before);
    return c * (after / before);
}

// tonemapping.wesl:106-140: row vector times the matrices of the columns given there.
float3 TonemapAcesFitted(float3 c)
{
    const float3 rrt = float3(dot(c, float3(0.59719f, 0.35458f, 0.04823f)), dot(c, float3(0.07600f, 0.90834f, 0.01566f)),
                              dot(c, float3(0.02840f, 0.13383f, 0.83777f)));
    const float3 fitted = (rrt * (rrt + 0.0245786f) - 0.000090537f) / (rrt * (0.983729f * rrt + 0.4329510f) + 0.238081f);
    const float3 odt = float3(dot(fitted, float3(1.60475f, -0.53108f, -0.07367f)), dot(fitted, float3(-0.10208f, 1.10813f, -0.00605f)),
                              dot(fitted, float3(-0.00327f, -0.07276f, 1.07602f)));
    return saturate(odt);
}

// tonemapping.wesl:48-56 (the branch in use) and 39-80.
float3 BoringCurve(float3 v)
{
    return 1.0f - exp(-v);
}

float3 TonemapSomewhatBoringDisplayTransform(float3 c)
{
    const float3 ycbcr = float3(dot(c, float3(0.2126f, 0.7152f, 0.0722f)), dot(c, float3(-0.1146f, -0.3854f, 0.5f)),
                                dot(c, float3(0.5f, -0.4542f, -0.0458f)));
    const float bt = BoringCurve(length(ycbcr.yz) * 2.4f).x;
    float desat = max((bt - 0.7f) * 0.8f, 0.0f);
    desat *= desat;
    const float3 desatColour = lerp(c, ycbcr.xxx, desat);
    const float tmLuma = BoringCurve(ycbcr.xxx).x;
    const float3 tm0 = c * max(0.0f, tmLuma / max(1e-5f, TonemappingLuminance(c)));
    const float3 tm1 = BoringCurve(desatColour);
    return lerp(tm0, tm1, bt * bt) * 0.97f;
}

// tonemapping.wesl:258-301.
float3 TonemapKhronosPbrNeutral(float3 c)
{
    const float startCompression = 0.8f - 0.04f;
    const float desaturation = 0.15f;
    const float x = min(c.r, min(c.g, c.b));
    const float offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;
    const float3 offsetColour = c - offset;
    const float peak = max(offsetColour.r, max(offsetColour.g, offsetColour.b));
    if (peak < startCompression)
    {
        return offsetColour;
    }
    const float d = 1.0f - startCompression;
    const float newPeak = 1.0f - d * d / (peak + d - startCompression);
    const float3 colour = offsetColour * (newPeak / peak);
    const float g = 1.0f - 1.0f / (desaturation * (peak - newPeak) + 1.0f);
    return lerp(colour, float3(newPeak, newPeak, newPeak), g);
}

// The method's curve on linear light, negative channels taken as 0 first (Bevy's tone_mapping with no colour
// grading, tonemapping.wesl:373-428). Not clamped.
float3 Tonemap(uint method, float3 linearLight)
{
    const float3 c = max(linearLight, 0.0f);
    switch (method)
    {
    case GYO_TONEMAP_REINHARD:
        return c / (1.0f + c);
    case GYO_TONEMAP_REINHARD_LUMINANCE:
        return TonemapReinhardLuminance(c);
    case GYO_TONEMAP_ACES_FITTED:
        return TonemapAcesFitted(c);
    case GYO_TONEMAP_SOMEWHAT_BORING_DISPLAY_TRANSFORM:
        return TonemapSomewhatBoringDisplayTransform(c);
    case GYO_TONEMAP_KHRONOS_PBR_NEUTRAL:
        return TonemapKhronosPbrNeutral(c);
    case GYO_TONEMAP_NONE:
    default:
        return c;
    }
}
#endif
