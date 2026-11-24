#ifndef GYO_RENDER_PASS2D_INCLUDED
#define GYO_RENDER_PASS2D_INCLUDED
// The 2D pass's shader ABI (docs/architecture/target/v2/render.zh-Hant.md, section 4; the C++ side is
// include/engine/render/ShaderAbi.hpp, Design Invariant RD-4). Register spaces follow SDL_GPU's HLSL convention
// (SDL_gpu.h, SDL_CreateGPUShader): vertex uniforms in space1, fragment textures and samplers in space2.
// float4 lanes only, so no target API packs the uniform differently.

// Vertex uniform slot 0: xy = scale, zw = offset (render's ViewTransform: clip = position * scale + offset).
cbuffer Pass2DView : register(b0, space1)
{
    float4 viewTransform;
};

// Fragment sampler slot 0: the draw's texture (the white texture for plain colours).
Texture2D<float4> passTexture : register(t0, space2);
SamplerState passSampler : register(s0, space2);

// Vertex input locations 0, 1, 2 (render's Vertex2D: position, uv, colour as 4 normalized bytes).
struct Pass2DVertex
{
    float2 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
};

struct Pass2DVarying
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
};
#endif
