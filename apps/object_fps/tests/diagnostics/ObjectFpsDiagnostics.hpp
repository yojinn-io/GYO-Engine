#pragma once

#include "HeadlessSmoke.hpp"
#include "MuzzleProbe.hpp"

#include <filesystem>

namespace Engine::Platform::Sdl { class SdlPlatform; }
namespace Engine::Render { class Renderer; class ShaderLibrary; }

// These checks are attached by the optional test/release adapters. Product-only
// builds do not include this header or compile any source in this directory.
int ValidatePackage(const std::filesystem::path& root,
                    const Engine::Render::ShaderLibrary& shaders);
int RunShaderProbe(Engine::Platform::Sdl::SdlPlatform& platform,
                   Engine::Render::Renderer& renderer);
