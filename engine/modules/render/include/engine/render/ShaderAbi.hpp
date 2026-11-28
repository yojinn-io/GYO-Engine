#pragma once

// The C++ side of the passes' shader ABI (docs/architecture/target/v2/render.zh-Hant.md, section 4; Design Invariant
// RD-4). The HLSL side is shaders/Pass2D.hlsli, Mesh3D.hlsli, Fullscreen.hlsli and Tonemapping.hlsli; a backend
// binds what is written here. Both sides change together.

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "engine/render/RenderCommands.hpp"
#include "engine/render/Tonemapping.hpp"

namespace GYO::Engine::Render::ShaderAbi {

// The name a backend carries a shader under: its source's file name without ".hlsl" (sdl_gpu's shader tool, --shader).
[[nodiscard]] constexpr std::string_view ShaderName(const std::string_view source) noexcept {
    constexpr std::string_view kSuffix = ".hlsl";
    return source.ends_with(kSuffix) ? source.substr(0, source.size() - kSuffix.size()) : source;
}

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

// ---- The 3D pass (render.zh-Hant.md section 8): unlit, the texture times the instance's colour.
inline constexpr std::string_view kMesh3DVertexSource = "mesh3d.vert.hlsl";
inline constexpr std::string_view kMesh3DFragmentSource = "mesh3d.frag.hlsl";

// Vertex input, buffer slot 0, one Vertex3D per vertex; the normal is not bound (unlit).
inline constexpr std::uint32_t kMesh3DVertexSlot = 0;
inline constexpr std::uint32_t kMesh3DVertexStride = sizeof(Vertex3D);
inline constexpr std::uint32_t kMesh3DPositionLocation = 0;  // float3
inline constexpr std::uint32_t kMesh3DUvLocation = 1;        // float2
inline constexpr std::uint32_t kMesh3DPositionOffset = offsetof(Vertex3D, position);
inline constexpr std::uint32_t kMesh3DUvOffset = offsetof(Vertex3D, uv);
static_assert(kMesh3DPositionOffset == 0 && kMesh3DUvOffset == 24);

// Vertex input, buffer slot 1, one Instance3D per instance (big-map declaration section 16). A backend binds the pass's
// instances and a draw names its range by its first instance (first_instance), which instance-rate inputs follow on
// every GPU API; the shader reads its instance through these inputs alone and never through an instance id (whose base
// differs between GPU APIs). The world is four float4s at locations 2 to 5: Matrix4's four columns in storage order,
// which HLSL's float4x4 constructor takes as its rows (mul(float4(p, 1), M), as the uniforms).
inline constexpr std::uint32_t kMesh3DInstanceSlot = 1;
inline constexpr std::uint32_t kMesh3DInstanceStride = sizeof(Instance3D);
inline constexpr std::uint32_t kMesh3DWorldLocation = 2;      // float4 x kMesh3DWorldColumns, column c at location 2 + c
inline constexpr std::uint32_t kMesh3DWorldColumns = 4;
inline constexpr std::uint32_t kMesh3DColorLocation = 6;      // 4 bytes, normalized to 0..1
inline constexpr std::uint32_t kMesh3DWorldOffset = offsetof(Instance3D, world);
inline constexpr std::uint32_t kMesh3DColorOffset = offsetof(Instance3D, color);
static_assert(kMesh3DWorldOffset == 0 && kMesh3DColorOffset == 64 && kMesh3DInstanceStride == 68);

// Vertex uniform slot 0: the pass's clipFromWorld, Matrix4's 16 floats in order (the HLSL side declares row_major
// float4x4 and computes mul(mul(float4(p, 1), world), clipFromWorld), engine/core/math's Matrix4.hpp).
inline constexpr std::uint32_t kMesh3DViewUniformSlot = 0;
struct Mesh3DViewUniform final {
    float values[16];
};
static_assert(sizeof(Mesh3DViewUniform) == 64);
[[nodiscard]] inline Mesh3DViewUniform ToUniform(const Math::Matrix4& clipFromWorld) noexcept {
    Mesh3DViewUniform u{};
    for (std::size_t i = 0; i < 16; ++i) u.values[i] = clipFromWorld.values[i];
    return u;
}

// Fragment sampler slot 0: the draw's texture. The colour comes with the instance (no fragment uniform).
inline constexpr std::uint32_t kMesh3DTextureSlot = 0;

inline constexpr std::uint32_t kMesh3DVertexUniformBuffers = 1;
inline constexpr std::uint32_t kMesh3DVertexSamplers = 0;
inline constexpr std::uint32_t kMesh3DFragmentUniformBuffers = 0;
inline constexpr std::uint32_t kMesh3DFragmentSamplers = 1;

// ---- The depth-only pass (render-pipeline declaration section 14): the 3D pass's vertex input and uniform, no colour.
inline constexpr std::string_view kDepthVertexSource = "depth.vert.hlsl";
inline constexpr std::string_view kDepthFragmentSource = "depth.frag.hlsl";
// Vertex input: the 3D pass's position (slot kMesh3DVertexSlot, location kMesh3DPositionLocation) and instance world
// (slot kMesh3DInstanceSlot, locations kMesh3DWorldLocation on); uv and colour are not bound. Vertex uniform slot 0:
// the pass's clipFromWorld as Mesh3DViewUniform.
inline constexpr std::uint32_t kDepthVertexUniformBuffers = 1;
inline constexpr std::uint32_t kDepthVertexSamplers = 0;
inline constexpr std::uint32_t kDepthFragmentUniformBuffers = 0;
inline constexpr std::uint32_t kDepthFragmentSamplers = 0;

// ---- The full-screen pass (render-pipeline declaration section 14): one triangle, its vertices 0, 1, 2 (no vertex
// input, no vertex uniform). Its fragment shader (PassFullscreen::shader) samples the pass's inputs at sampler slots
// kFullscreenFirstInputSlot, ... in order and reads its parameters, when it has some, at fragment uniform slot
// kFullscreenParametersSlot (the cbuffer at b0, space3).
inline constexpr std::string_view kFullscreenVertexSource = "fullscreen.vert.hlsl";
inline constexpr std::uint32_t kFullscreenVertices = 3;
inline constexpr std::uint32_t kFullscreenVertexUniformBuffers = 0;
inline constexpr std::uint32_t kFullscreenVertexSamplers = 0;
inline constexpr std::uint32_t kFullscreenFirstInputSlot = 0;
inline constexpr std::uint32_t kFullscreenParametersSlot = 0;
// A full-screen fragment shader declares at most one uniform buffer (its parameters).
inline constexpr std::uint32_t kFullscreenFragmentUniformBuffersAtMost = 1;

// ---- The unlit 3D pass on a linear target (render-pipeline declaration section 16): mesh3d.frag's colour, computed on
// the bytes as on an 8-bit target, decoded to the linear light it stands for (shaders/Color.hlsli). The 3D pass's
// vertex stage, inputs and texture as kMesh3D*.
inline constexpr std::string_view kMesh3DLinearFragmentSource = "mesh3d_linear.frag.hlsl";

// ---- The tonemapping pass (render-pipeline declaration section 16; Tonemapping.hpp): a full-screen fragment shader of
// the Engine's own. Its one input is the HDR picture, sampled at the pass's own pixel (the picture is as large as the
// target); its parameters one float4, lane 0 the method's value (Tonemapping::Method; shaders/Tonemapping.hlsli
// names the same values), lanes 1 to 3 zero. It writes the curve's result, clamped and sRGB-encoded, and the
// picture's alpha.
inline constexpr std::string_view kTonemappingFragmentSource = "tonemapping.frag.hlsl";
inline constexpr std::uint32_t kTonemappingInputs = 1;
struct TonemappingParameters final {
    float lanes[4];
};
static_assert(sizeof(TonemappingParameters) == 16);
[[nodiscard]] constexpr TonemappingParameters ToParameters(const Tonemapping::Method method) noexcept {
    return TonemappingParameters{{static_cast<float>(static_cast<std::uint8_t>(method)), 0.0F, 0.0F, 0.0F}};
}
static_assert(static_cast<std::uint8_t>(Tonemapping::Method::None) == 1 && static_cast<std::uint8_t>(Tonemapping::Method::Reinhard) == 2 &&
              static_cast<std::uint8_t>(Tonemapping::Method::ReinhardLuminance) == 3 &&
              static_cast<std::uint8_t>(Tonemapping::Method::AcesFitted) == 4 &&
              static_cast<std::uint8_t>(Tonemapping::Method::SomewhatBoringDisplayTransform) == 5 &&
              static_cast<std::uint8_t>(Tonemapping::Method::KhronosPbrNeutral) == 6,
              "Tonemapping.hlsli's GYO_TONEMAP_* values");

} // namespace GYO::Engine::Render::ShaderAbi
