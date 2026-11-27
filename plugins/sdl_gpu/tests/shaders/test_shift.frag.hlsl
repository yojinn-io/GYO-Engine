// A full-screen pass's fragment shader of the tests (sdl_gpu.targets): its one input moved by the parameters' first
// two lanes (in uv: a pixel at uv shows the input at uv - offset.xy).
#include "Fullscreen.hlsli"

Texture2D<float4> source : register(t0, space2);
SamplerState sourceSampler : register(s0, space2);

cbuffer Shift : register(b0, space3)
{
    float4 offset;
};

float4 main(FullscreenVarying input) : SV_Target0
{
    return source.Sample(sourceSampler, input.uv - offset.xy);
}
