#ifndef GYO_RASTER_ABI_INCLUDED
#define GYO_RASTER_ABI_INCLUDED
// gyo.raster.v1: float4 lanes avoid layout differences between target APIs.
// Logical resource locations are supplied by the offline toolchain adapter.
#if defined(GYO_VERTEX_UNLIT)
cbuffer VertexUniforms : register(GYO_VERTEX_UNIFORM_SLOT, GYO_VERTEX_UNIFORM_SPACE)
{
    row_major float4x4 worldViewProjection;
};
#endif
#if defined(GYO_FRAGMENT_UNLIT)
Texture2D<float4> colorTexture : register(GYO_FRAGMENT_TEXTURE_SLOT, GYO_FRAGMENT_IMAGE_SPACE);
SamplerState colorSampler : register(GYO_FRAGMENT_SAMPLER_SLOT, GYO_FRAGMENT_IMAGE_SPACE);
cbuffer FragmentUniforms : register(GYO_FRAGMENT_UNIFORM_SLOT, GYO_FRAGMENT_UNIFORM_SPACE)
{
    float4 tint;
    float4 uvScaleOffset;
    float4 alphaParams; // x = alpha cutoff, negative disables clipping.
};
float4 GyoSampleUnlit(float2 uv)
{
    const float4 color = colorTexture.Sample(colorSampler,
        uv * uvScaleOffset.xy + uvScaleOffset.zw) * tint;
    if (alphaParams.x >= 0.0f) clip(color.a - alphaParams.x);
    return color;
}
#endif
#if defined(GYO_FRAGMENT_SCENE_POST)
Texture2D<float4> sceneTexture : register(GYO_FRAGMENT_TEXTURE_SLOT, GYO_FRAGMENT_IMAGE_SPACE);
SamplerState sceneSampler : register(GYO_FRAGMENT_SAMPLER_SLOT, GYO_FRAGMENT_IMAGE_SPACE);
cbuffer SceneColorUniforms : register(GYO_FRAGMENT_UNIFORM_SLOT, GYO_FRAGMENT_UNIFORM_SPACE)
{
    float4 sceneColorParams; // x = exposure EV, y = gamma adjustment.
};
#endif
#endif
