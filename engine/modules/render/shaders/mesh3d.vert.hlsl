// The 3D pass, vertex stage: local to clip space; uv passes through.
#include "Mesh3D.hlsli"

Mesh3DVarying main(Mesh3DVertex input)
{
    Mesh3DVarying output;
    output.position = mul(float4(input.position, 1.0f), clipFromLocal);
    output.uv = input.uv;
    return output;
}
