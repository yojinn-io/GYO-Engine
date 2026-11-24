// The 2D pass, vertex stage: view units to clip space; uv and colour pass through.
#include "Pass2D.hlsli"

Pass2DVarying main(Pass2DVertex input)
{
    Pass2DVarying output;
    output.position = float4(input.position * viewTransform.xy + viewTransform.zw, 0.0f, 1.0f);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
