// The tonemapping pass, fragment stage (render-pipeline declaration section 16; ShaderAbi.hpp's kTonemapping*): the
// HDR picture's light at this pixel (the picture is as large as the target; the nearest texel), through the method's
// curve, clamped and sRGB-encoded for the 8-bit target; alpha as the picture's.
#include "Fullscreen.hlsli"
#include "Color.hlsli"
#include "Tonemapping.hlsli"

Texture2D<float4> picture : register(t0, space2);
SamplerState pictureSampler : register(s0, space2);

cbuffer TonemappingParameters : register(b0, space3)
{
    // x: Tonemapping::Method's value; y, z, w: 0.
    float4 tonemapping;
};

float4 main(FullscreenVarying input) : SV_Target0
{
    float width;
    float height;
    picture.GetDimensions(width, height);
    const float4 light = picture.Sample(pictureSampler, input.position.xy / float2(width, height));
    const float3 mapped = Tonemap((uint)tonemapping.x, light.rgb);
    return float4(LinearToSrgb(mapped), saturate(light.a));
}
