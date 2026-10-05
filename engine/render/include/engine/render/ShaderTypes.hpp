#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Render {

enum class ShaderFormat : std::uint32_t { DXIL = 1, SPIRV = 2, Metallib = 4 };
using ShaderFormatMask = std::uint32_t;
constexpr ShaderFormatMask FormatBit(ShaderFormat format) noexcept {
    return static_cast<ShaderFormatMask>(format);
}
inline constexpr ShaderFormatMask AllShaderFormats = 7;
enum class ShaderStage { Vertex, Fragment };

// Versioned CPU/GPU data contract. Physical resource bindings belong to the
// shader toolchain profile; no external compiler or graphics types escape it.
inline constexpr std::string_view ShaderAbiVersion = "gyo.raster.v1";

struct ShaderResourceLayout final {
    std::uint32_t samplers{};
    std::uint32_t storageTextures{};
    std::uint32_t storageBuffers{};
    std::vector<std::uint32_t> uniformBufferSizes;
};

struct ShaderArtifact final {
    ShaderFormat format{ShaderFormat::SPIRV};
    ShaderStage stage{ShaderStage::Vertex};
    std::string entrypoint;
    ShaderResourceLayout resources;
    std::vector<std::byte> code;
};

struct ShaderProgram final {
    std::string interfaceName;
    std::shared_ptr<const ShaderArtifact> vertex;
    std::shared_ptr<const ShaderArtifact> fragment;
};

[[nodiscard]] constexpr std::string_view ShaderFormatName(ShaderFormat format) noexcept {
    switch (format) {
    case ShaderFormat::DXIL: return "dxil";
    case ShaderFormat::SPIRV: return "spirv";
    case ShaderFormat::Metallib: return "metallib";
    }
    return "unknown";
}

} // namespace Engine::Render
