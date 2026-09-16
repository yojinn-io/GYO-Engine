#define GYO_FRAGMENT_SCENE_POST
#include "RasterAbi.hlsli"
// The sRGB target performs the only linear-to-sRGB conversion.
float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    const float3 sceneLinear = max(sceneTexture.Sample(sceneSampler, uv).rgb, 0.0f);
    const float3 exposed = sceneLinear * exp2(sceneColorParams.x);
    return float4(pow(saturate(exposed), rcp(sceneColorParams.y)), 1.0f);
}
