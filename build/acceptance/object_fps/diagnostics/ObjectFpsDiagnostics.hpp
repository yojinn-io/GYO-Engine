#pragma once

#include "HeadlessSmoke.hpp"
#include "MuzzleProbe.hpp"

namespace Engine::Asset { class AssetCatalog; }
namespace Engine::Platform::Sdl { class SdlPlatform; }
namespace Engine::Render { class Renderer; class ShaderLibrary; }

// Independent acceptance checks; never compiled into the product executable.
int ValidatePackage(const Engine::Asset::AssetCatalog& catalog,
                    const Engine::Render::ShaderLibrary& shaders);
int RunShaderProbe(Engine::Platform::Sdl::SdlPlatform& platform,
                   Engine::Render::Renderer& renderer);
