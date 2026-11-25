#ifndef GYO_RENDER_MESH3D_INCLUDED
#define GYO_RENDER_MESH3D_INCLUDED
// The 3D pass's shader ABI (docs/architecture/target/v2/render.zh-Hant.md, section 8; the C++ side is
// include/engine/render/ShaderAbi.hpp, Design Invariant RD-4). Register spaces follow SDL_GPU's HLSL convention:
// vertex uniforms in space1, fragment textures and samplers in space2.

// Vertex uniform slot 0: the pass's clip from world, Matrix4's floats in order, used as mul(float4(p, 1), M).
cbuffer Mesh3DView : register(b0, space1)
{
    row_major float4x4 clipFromWorld;
};

// Fragment sampler slot 0: the draw's texture (the white texture for none).
Texture2D<float4> meshTexture : register(t0, space2);
SamplerState meshSampler : register(s0, space2);

// Vertex input locations 0, 1 (render's Vertex3D: position, uv; the normal is not bound), per vertex; locations 2 to 6
// (render's Instance3D: the world's four columns as Matrix4 stores them, the colour), per instance.
struct Mesh3DVertex
{
    float3 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 world0 : TEXCOORD2;
    float4 world1 : TEXCOORD3;
    float4 world2 : TEXCOORD4;
    float4 world3 : TEXCOORD5;
    float4 color : TEXCOORD6;
};

struct Mesh3DVarying
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    // One value for the whole instance: not interpolated.
    nointerpolation float4 color : TEXCOORD1;
};
#endif
