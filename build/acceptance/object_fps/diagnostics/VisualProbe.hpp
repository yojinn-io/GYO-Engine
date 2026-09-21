#pragma once
#include <filesystem>
#include <memory>
namespace Engine::Platform::Sdl { class SdlPlatform; }
namespace Engine::Render { class Renderer; }
namespace Engine::Asset { class AssetManager; }
namespace fps { class ObjectFpsPresentation; class CampaignContent; }
int RunVisualProbe(Engine::Platform::Sdl::SdlPlatform&, Engine::Render::Renderer&,
    fps::ObjectFpsPresentation&, Engine::Asset::AssetManager&,
    const std::shared_ptr<const fps::CampaignContent>&, bool smoke, bool reloadSequence,
    const std::filesystem::path& captureDirectory, float width, float height);
