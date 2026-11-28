// The 3D pass on a linear target (a Hdr camera's picture; render-pipeline declaration 16.8), fragment stage: unlit, the
// texture times the instance's colour, each as linear light (LinearColour.hlsli), alpha straight.
#include "Mesh3D.hlsli"
#include "LinearColour.hlsli"

float4 main(Mesh3DVarying input) : SV_Target0
{
    return LinearTimes(meshTexture.Sample(meshSampler, input.uv), input.color);
}
