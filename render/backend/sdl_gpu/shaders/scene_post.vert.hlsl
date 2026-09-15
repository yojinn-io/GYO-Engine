// Full-screen triangle used to composite the linear scene into the SDR
// swapchain. No vertex buffers or vertex uniforms are required.
struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput main(uint vertexId : SV_VertexID)
{
    const float2 uv = float2((vertexId << 1) & 2, vertexId & 2);

    VertexOutput output;
    output.position = float4(
        uv.x * 2.0f - 1.0f,
        1.0f - uv.y * 2.0f,
        0.0f,
        1.0f);
    output.uv = uv;
    return output;
}
