#pragma once

// The shaders sdl_gpu carries inside it (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_gpu", the shader tool):
// render's HLSL compiled at build time by sdl_gpu's shader tool to this platform's formats and written into a
// generated C++ source (in the build directory) that defines EmbeddedShaders(). In GYO's words: no SDL type, so the
// generated source is not adapter code (Design Invariant SDL-1). Dear ImGui's SDL_GPU backend carries its shaders the
// same way (bytes in a source file).

#include <cstdint>
#include <span>
#include <string_view>

namespace GYO::Plugins::SDLGPU {

enum class ShaderFormat : std::uint8_t {
    Spirv = 1,
    Dxil,
    Metallib,
};

enum class ShaderStage : std::uint8_t {
    Vertex = 1,
    Fragment,
};

// The shader's code in one format, with its entry point in that format (SPIRV-Cross names "main" "main0" in Metal).
struct CompiledShader final {
    ShaderFormat format;
    std::span<const std::uint8_t> code;
    std::string_view entryPoint;
};

// What the shader uses, read from its SPIR-V by the tool (SDL_GPUShaderCreateInfo wants it).
struct ShaderResources final {
    std::uint32_t samplers = 0;
    std::uint32_t storageTextures = 0;
    std::uint32_t storageBuffers = 0;
    std::uint32_t uniformBuffers = 0;
};

struct EmbeddedShader final {
    // The HLSL file's name without ".hlsl" (render's ShaderAbi.hpp names the files).
    std::string_view name;
    ShaderStage stage;
    ShaderResources resources;
    std::span<const CompiledShader> compiled;
};

// Every shader of this build (defined in the generated source).
[[nodiscard]] std::span<const EmbeddedShader> EmbeddedShaders() noexcept;

// The shader of this name, or nullptr.
[[nodiscard]] inline const EmbeddedShader* FindEmbeddedShader(const std::string_view name) noexcept {
    for (const EmbeddedShader& shader : EmbeddedShaders()) {
        if (shader.name == name) return &shader;
    }
    return nullptr;
}

} // namespace GYO::Plugins::SDLGPU
