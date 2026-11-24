// Does not compile: the shader tool must reject it and leave its output alone (sdl_gpu.shader_tool.rejects).
float4 main(float4 position : SV_Position) : SV_Target0
{
    return undefinedValue;
}
