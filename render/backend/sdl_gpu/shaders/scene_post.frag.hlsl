// Applies display-relative scene adjustments in linear space. The target is
// an sRGB swapchain texture, so the render target performs the one and only
// standard linear-to-sRGB conversion after this shader returns.
Texture2D<float4> sceneTexture : register(t0, space2);
SamplerState sceneSampler : register(s0, space2);

cbuffer SceneColorUniforms : register(b0, space3)
{
    float exposureEv;
    float gammaAdjustment;
    float2 sceneColorPadding;
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    const float3 sceneLinear = max(
        sceneTexture.Sample(sceneSampler, uv).rgb,
        0.0f);
    const float3 exposed = sceneLinear * exp2(exposureEv);
    const float3 adjusted = pow(
        saturate(exposed),
        rcp(gammaAdjustment));
    return float4(adjusted, 1.0f);
}
