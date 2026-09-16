#pragma once

#include <filesystem>

namespace Engine::Asset { class AssetManager; }
namespace Engine::Platform::Sdl { class SdlPlatform; }
namespace Engine::Render { class IRenderDevice; class Renderer; }
namespace fps { class CampaignContent; }

// Bounded GPU readback check. Never used by ordinary game presentation.
int RunMuzzleProbe(
    Engine::Platform::Sdl::SdlPlatform& platform,
    Engine::Render::IRenderDevice& device,
    Engine::Render::Renderer& renderer,
    Engine::Asset::AssetManager& assets,
    const fps::CampaignContent& content,
    const std::filesystem::path& captureDirectory);
