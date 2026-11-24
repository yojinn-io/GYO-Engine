// The 2D pass, fragment stage: the texture times the vertex colour, alpha straight.
#include "Pass2D.hlsli"

float4 main(Pass2DVarying input) : SV_Target0
{
    return passTexture.Sample(passSampler, input.uv) * input.color;
}
