// The 2D pass on a linear target (render-pipeline declaration 16.8), fragment stage: the texture times the vertex
// colour, each as linear light (LinearColour.hlsli), alpha straight.
#include "Pass2D.hlsli"
#include "LinearColour.hlsli"

float4 main(Pass2DVarying input) : SV_Target0
{
    return LinearTimes(passTexture.Sample(passSampler, input.uv), input.color);
}
