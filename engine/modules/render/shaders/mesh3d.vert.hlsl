// The 3D pass, vertex stage: local to world by the instance's world, world to clip by the pass's; uv and the
// instance's colour pass through.
#include "Mesh3D.hlsli"

Mesh3DVarying main(Mesh3DVertex input)
{
    // The rows as Matrix4 stores them (float4x4's constructor takes rows).
    const float4x4 world = float4x4(input.world0, input.world1, input.world2, input.world3);
    Mesh3DVarying output;
    output.position = mul(mul(float4(input.position, 1.0f), world), clipFromWorld);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
