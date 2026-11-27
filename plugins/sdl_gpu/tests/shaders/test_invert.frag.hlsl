// A full-screen pass's fragment shader of the tests (sdl_gpu.targets): its one input with the colour turned round
// (1 - rgb), alpha as it is. No parameters.
#include "Fullscreen.hlsli"

Texture2D<float4> source : register(t0, space2);
SamplerState sourceSampler : register(s0, space2);

float4 main(FullscreenVarying input) : SV_Target0
{
    const float4 c = source.Sample(sourceSampler, input.uv);
    return float4(1.0f - c.rgb, c.a);
}
