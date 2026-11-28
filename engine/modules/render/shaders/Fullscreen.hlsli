#ifndef GYO_RENDER_FULLSCREEN_INCLUDED
#define GYO_RENDER_FULLSCREEN_INCLUDED
// The full-screen pass's shader ABI (render-pipeline declaration section 14; the C++ side is
// include/engine/render/ShaderAbi.hpp, Design Invariant RD-4). Register spaces follow SDL_GPU's HLSL convention
// (SDL_gpu.h, SDL_CreateGPUShader): fragment textures and samplers in space2, fragment uniforms in space3.
//
// The vertex stage is render's fullscreen.vert (no vertex input; Bevy's FullscreenShader). A pass's fragment shader
// takes FullscreenVarying; its inputs (PassFullscreen::inputs) are its textures and samplers t0 / s0, t1 / s1, ... in
// order, in space2; its parameters (PassFullscreen::parameters), when it has some, are the cbuffer at b0 in space3,
// float4 lanes only, so no target API packs them differently.

struct FullscreenVarying
{
    float4 position : SV_Position;
    // (0, 0) at the top-left corner of the pass's viewport (the whole target when it has none), (1, 1) at its
    // bottom-right one, y down (as Pass2D's uv).
    float2 uv : TEXCOORD0;
};
#endif
