#pragma once
#include <cstddef>
#include "render/RenderTypes.hpp"
namespace Engine::Render::ShaderAbi {
struct Matrix4 final { float values[4][4]{}; };
struct alignas(16) VertexUniforms final { Matrix4 worldViewProjection{}; };
struct alignas(16) FragmentUniforms final {
    float tint[4]{};
    float uvScaleOffset[4]{};
    // The following four floats occupy HLSL float4 alphaParams at offset 32.
    float alphaCutoff{-1.0F};
    float padding[3]{};
};
struct alignas(16) SceneColorUniforms final {
    // One HLSL float4 sceneColorParams lane; only x/y carry values.
    float exposureEv{};
    float gammaAdjustment{1.0F};
    float padding[2]{};
};
static_assert(sizeof(Vertex3D) == 20 && offsetof(Vertex3D, uv) == 12);
static_assert(sizeof(VertexUniforms) == 64);
static_assert(sizeof(FragmentUniforms) == 48);
static_assert(offsetof(FragmentUniforms, alphaCutoff) == 32);
static_assert(sizeof(SceneColorUniforms) == 16);
} // namespace Engine::Render::ShaderAbi
