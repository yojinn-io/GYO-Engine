#pragma once

#include <optional>
#include <string>
#include <vector>
#include "render/RenderTypes.hpp"
#include "render/ShaderTypes.hpp"

namespace Engine::Render {

enum class TextureFormat { Rgba8Unorm, Rgba8SRgb, Bgra8Unorm, Bgra8SRgb, Rgba16Float, Depth32Float };
struct TextureDesc final {
    std::uint32_t width{}, height{};
    TextureFormat format{TextureFormat::Rgba8Unorm};
    bool sampled{true};
    bool colorTarget{};
    bool depthTarget{};
};
enum class CullMode { None, Front, Back };
struct PipelineDesc final {
    ShaderHandle vertexShader{}, fragmentShader{};
    TextureFormat colorFormat{TextureFormat::Rgba16Float};
    bool vertexInput{true};
    CullMode cull{CullMode::Back};
    bool blend{};
    bool depthTest{true};
    bool depthWrite{true};
    // GYO's +Z-forward, +Y-up projection presents outward mesh faces clockwise.
    bool clockwiseFrontFace{true};
};
struct RenderDeviceInfo final {
    ShaderFormatMask shaderFormats{};
    std::string driver;
};
// A single outstanding acquired frame, owned by the device. Its token is
// consumed exactly once by SubmitFrame or AbandonFrame; no native types leak.
struct AcquiredFrame final {
    std::uint64_t token{};
    std::uint32_t width{}, height{};
    TextureFormat colorFormat{TextureFormat::Bgra8SRgb};
};
enum class AttachmentLoad { Clear, Load, DontCare };
struct TextureBinding final {
    TextureHandle texture{};
    SamplerMode sampler{SamplerMode::LinearClamp};
};
struct PreparedDraw final {
    PipelineHandle pipeline{};
    MeshHandle mesh{}; // invalid uses non-indexed vertexCount (fullscreen triangle)
    std::uint32_t vertexCount{3};
    std::vector<TextureBinding> fragmentTextures;
    std::vector<std::vector<std::byte>> vertexUniforms;
    std::vector<std::vector<std::byte>> fragmentUniforms;
};
struct PreparedPass final {
    TextureHandle color{}; // invalid selects this acquired frame's swapchain
    TextureHandle depth{};
    AttachmentLoad colorLoad{AttachmentLoad::Load};
    AttachmentLoad depthLoad{AttachmentLoad::Clear};
    Color clearColor{};
    float clearDepth{1.0F};
    bool storeColor{true};
    bool storeDepth{};
    std::vector<PreparedDraw> draws;
};
struct PreparedFrame final { std::vector<PreparedPass> passes; };
struct TextureReadback final {
    std::uint32_t width{}, height{}, rowPitch{};
    TextureFormat format{TextureFormat::Rgba8Unorm};
    std::vector<std::byte> bytes;
};

} // namespace Engine::Render
