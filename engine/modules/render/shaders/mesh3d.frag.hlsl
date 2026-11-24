// The 3D pass, fragment stage: unlit, the texture times the draw's colour, alpha straight.
#include "Mesh3D.hlsli"

float4 main(Mesh3DVarying input) : SV_Target0
{
    return meshTexture.Sample(meshSampler, input.uv) * drawColor;
}
