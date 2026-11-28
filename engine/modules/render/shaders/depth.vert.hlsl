// The depth-only pass, vertex stage: the 3D pass's vertex stage without uv and colour. It takes the 3D pass's whole
// vertex input (Mesh3DVertex: the shader compiler numbers the inputs in the order they are declared, so a struct that
// left uv out would move the world to uv's location) and the pass's clip from world at the 3D pass's uniform slot.
#include "Mesh3D.hlsli"

float4 main(Mesh3DVertex input) : SV_Position
{
    // As mesh3d.vert: Matrix4's four columns in storage order are float4x4's rows (mul(v, M)).
    const float4x4 world = float4x4(input.world0, input.world1, input.world2, input.world3);
    return mul(mul(float4(input.position, 1.0f), world), clipFromWorld);
}
