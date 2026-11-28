// The 3D pass on a linear target (a Hdr camera's picture; render-pipeline declaration section 16), fragment stage:
// unlit, the texture times the instance's colour on the bytes as mesh3d.frag computes them for an 8-bit target, then
// decoded to the linear light they stand for, alpha straight. Through the identity curve its pixels encode back to
// mesh3d.frag's.
#include "Mesh3D.hlsli"
#include "Color.hlsli"

float4 main(Mesh3DVarying input) : SV_Target0
{
    const float4 c = meshTexture.Sample(meshSampler, input.uv) * input.color;
    return float4(SrgbToLinear(c.rgb), c.a);
}
