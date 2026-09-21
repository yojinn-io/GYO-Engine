#pragma once

#include "HeadlessSmoke.hpp"
#include "MuzzleProbe.hpp"

namespace Engine::Asset { class AssetCatalog; }
namespace Engine::Render { class ShaderLibrary; }

// Independent acceptance checks; never compiled into the product executable.
int ValidatePackage(const Engine::Asset::AssetCatalog& catalog,
                    const Engine::Render::ShaderLibrary& shaders);
