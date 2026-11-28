// A full-screen pass's fragment shader of the tests (sdl_gpu.hdr): writes its parameters' first float4 at every pixel
// (light past 1 on a Rgba16Float picture). No input.
#include "Fullscreen.hlsli"

cbuffer Constant : register(b0, space3)
{
    float4 colour;
};

float4 main(FullscreenVarying input) : SV_Target0
{
    return colour;
}
