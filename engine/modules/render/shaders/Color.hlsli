#ifndef GYO_RENDER_COLOR_INCLUDED
#define GYO_RENDER_COLOR_INCLUDED
// The sRGB transfer function (IEC 61966-2-1), exact, per channel: the C++ side is include/engine/render/Tonemapping.hpp
// (SrgbToLinear, LinearToSrgb; Design Invariant RD-4). step and lerp pick the branch (a vector ?: means another thing
// in each HLSL version).

float3 SrgbToLinear(float3 encoded)
{
    const float3 low = encoded / 12.92f;
    const float3 high = pow((max(encoded, 0.0f) + 0.055f) / 1.055f, 2.4f);
    return lerp(high, low, step(encoded, 0.04045f));
}

float3 LinearToSrgb(float3 linearLight)
{
    const float3 c = saturate(linearLight);
    const float3 low = c * 12.92f;
    const float3 high = 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
    return lerp(high, low, step(c, 0.0031308f));
}
#endif
