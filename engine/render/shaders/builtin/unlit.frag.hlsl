#define GYO_FRAGMENT_UNLIT
#include "RasterAbi.hlsli"
float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    return GyoSampleUnlit(uv);
}
