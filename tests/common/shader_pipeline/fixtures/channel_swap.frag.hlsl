#define GYO_FRAGMENT_UNLIT
#include "RasterAbi.hlsli"

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    const float4 color = GyoSampleUnlit(uv);
    return float4(color.b, color.g, color.r, color.a);
}
