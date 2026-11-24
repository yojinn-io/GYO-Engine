#pragma once

// The C++ side of the 2D pass's shader ABI (docs/architecture/target/v2/render.zh-Hant.md, section 4; Design
// Invariant RD-4). The HLSL side is shaders/Pass2D.hlsli; a backend binds what is written here. Both sides change
// together.

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "engine/render/RenderCommands.hpp"

namespace GYO::Engine::Render::ShaderAbi {

// The shaders' sources, under render's shaders/ directory; entry point "main" in both.
inline constexpr std::string_view kPass2DVertexSource = "pass2d.vert.hlsl";
inline constexpr std::string_view kPass2DFragmentSource = "pass2d.frag.hlsl";

// Vertex input: one buffer of Vertex2D.
inline constexpr std::uint32_t kPass2DVertexStride = sizeof(Vertex2D);
inline constexpr std::uint32_t kPass2DPositionLocation = 0;  // float2
inline constexpr std::uint32_t kPass2DUvLocation = 1;        // float2
inline constexpr std::uint32_t kPass2DColorLocation = 2;     // 4 bytes, normalized to 0..1
inline constexpr std::uint32_t kPass2DPositionOffset = offsetof(Vertex2D, position);
inline constexpr std::uint32_t kPass2DUvOffset = offsetof(Vertex2D, uv);
inline constexpr std::uint32_t kPass2DColorOffset = offsetof(Vertex2D, color);
static_assert(kPass2DPositionOffset == 0 && kPass2DUvOffset == 8 && kPass2DColorOffset == 16);

// Vertex uniform slot 0: ViewTransform as one float4 (scale.x, scale.y, offset.x, offset.y).
inline constexpr std::uint32_t kPass2DViewUniformSlot = 0;
struct Pass2DViewUniform final {
    float lanes[4];
};
static_assert(sizeof(Pass2DViewUniform) == 16);
[[nodiscard]] constexpr Pass2DViewUniform ToUniform(const ViewTransform& t) noexcept {
    return Pass2DViewUniform{{t.scale.x, t.scale.y, t.offset.x, t.offset.y}};
}

// Fragment sampler slot 0: the draw's texture.
inline constexpr std::uint32_t kPass2DTextureSlot = 0;

// What each stage uses (a backend states it when it makes the shader; the shader tool checks the compiled shader
// against it).
inline constexpr std::uint32_t kPass2DVertexUniformBuffers = 1;
inline constexpr std::uint32_t kPass2DVertexSamplers = 0;
inline constexpr std::uint32_t kPass2DFragmentUniformBuffers = 0;
inline constexpr std::uint32_t kPass2DFragmentSamplers = 1;

// ---- The 3D pass (render.zh-Hant.md section 8): unlit, the texture times the draw's colour.
inline constexpr std::string_view kMesh3DVertexSource = "mesh3d.vert.hlsl";
inline constexpr std::string_view kMesh3DFragmentSource = "mesh3d.frag.hlsl";

// Vertex input: one buffer of Vertex3D; the normal is not bound (unlit).
inline constexpr std::uint32_t kMesh3DVertexStride = sizeof(Vertex3D);
inline constexpr std::uint32_t kMesh3DPositionLocation = 0;  // float3
inline constexpr std::uint32_t kMesh3DUvLocation = 1;        // float2
inline constexpr std::uint32_t kMesh3DPositionOffset = offsetof(Vertex3D, position);
inline constexpr std::uint32_t kMesh3DUvOffset = offsetof(Vertex3D, uv);
static_assert(kMesh3DPositionOffset == 0 && kMesh3DUvOffset == 24);

// Vertex uniform slot 0: clip from local (clipFromWorld times the draw's world), Matrix4's 16 floats in order (the
// HLSL side declares row_major float4x4 and computes mul(float4(p, 1), M), engine/core/math's Matrix4.hpp).
inline constexpr std::uint32_t kMesh3DTransformUniformSlot = 0;
struct Mesh3DTransformUniform final {
    float values[16];
};
static_assert(sizeof(Mesh3DTransformUniform) == 64);
[[nodiscard]] inline Mesh3DTransformUniform ToUniform(const Math::Matrix4& clipFromLocal) noexcept {
    Mesh3DTransformUniform u{};
    for (std::size_t i = 0; i < 16; ++i) u.values[i] = clipFromLocal.values[i];
    return u;
}

// Fragment uniform slot 0: the draw's colour as a float4 (bytes / 255). Fragment sampler slot 0: the draw's texture.
inline constexpr std::uint32_t kMesh3DColorUniformSlot = 0;
struct Mesh3DColorUniform final {
    float lanes[4];
};
static_assert(sizeof(Mesh3DColorUniform) == 16);
[[nodiscard]] constexpr Mesh3DColorUniform ToUniform(const Color8 c) noexcept {
    return Mesh3DColorUniform{{c.r / 255.0F, c.g / 255.0F, c.b / 255.0F, c.a / 255.0F}};
}
inline constexpr std::uint32_t kMesh3DTextureSlot = 0;

inline constexpr std::uint32_t kMesh3DVertexUniformBuffers = 1;
inline constexpr std::uint32_t kMesh3DVertexSamplers = 0;
inline constexpr std::uint32_t kMesh3DFragmentUniformBuffers = 1;
inline constexpr std::uint32_t kMesh3DFragmentSamplers = 1;

} // namespace GYO::Engine::Render::ShaderAbi
