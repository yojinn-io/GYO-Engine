// The full-screen pass, vertex stage: one triangle over the whole target, drawn with vertices 0, 1, 2 and no vertex
// input (Bevy's fullscreen_vertex_shader, bevy_core_pipeline/src/fullscreen_vertex_shader.wesl:25-32). Vertex 0 is clip
// (-1, 1) at uv (0, 0), vertex 1 clip (-1, -3) at uv (0, 2), vertex 2 clip (3, 1) at uv (2, 0): the target's
// rectangle is the triangle's corner, uv (1, 1) at its bottom-right.
#include "Fullscreen.hlsli"

FullscreenVarying main(uint vertex : SV_VertexID)
{
    const float2 uv = float2(float(vertex >> 1), float(vertex & 1u)) * 2.0f;
    FullscreenVarying output;
    output.position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.uv = uv;
    return output;
}
