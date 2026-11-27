#pragma once

// The tests' own shaders (shaders/*.hlsl), compiled at build time by sdl_gpu's shader tool with --function TestShaders
// into a generated source of the test executable (tests/CMakeLists.txt); handed to GPUDevice::Create as its more.

#include <span>

#include "EmbeddedShaders.hpp"

namespace GYO::Plugins::SDLGPU {

[[nodiscard]] std::span<const EmbeddedShader> TestShaders() noexcept;

} // namespace GYO::Plugins::SDLGPU
