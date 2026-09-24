#pragma once

#include "RetroFPS/Game/GameSession.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace Engine::Asset { class AssetCatalog; class AssetManager; }
namespace Engine::Platform::Sdl { class SdlPlatform; }
namespace Engine::Render {
class Renderer;
class ShaderLibrary;
namespace Backend::SdlGpu { class SdlGpuRenderDevice; }
}

namespace fps {
class ObjectFpsPresentation;
class ObjectFpsRuntimeClient;

struct ObjectFpsApplicationOptions final {
    std::string title{"Object_FPS PVP"};
    std::string gpuDriver{"auto"};
    int width{1280};
    int height{720};
    GameSessionConfig session;
};

// Game-owned composition shared by the game, its design preview and acceptance
// executable. Lifecycle and gameplay stay here; validation policy stays outside.
class ObjectFpsApplication final {
public:
    ObjectFpsApplication();
    ~ObjectFpsApplication();
    ObjectFpsApplication(const ObjectFpsApplication&) = delete;
    ObjectFpsApplication& operator=(const ObjectFpsApplication&) = delete;

    [[nodiscard]] bool InitializeContent(const std::filesystem::path& assetRoot, std::string& error);
    [[nodiscard]] bool InitializeGraphics(const ObjectFpsApplicationOptions& options, std::string& error);
    [[nodiscard]] int Run();
    [[nodiscard]] const Engine::Asset::AssetCatalog& Catalog() const;
    [[nodiscard]] Engine::Asset::AssetManager& Assets();
    [[nodiscard]] const std::shared_ptr<const CampaignContent>& Content() const;
    [[nodiscard]] Engine::Render::ShaderLibrary& Shaders();
    [[nodiscard]] Engine::Platform::Sdl::SdlPlatform& Platform();
    [[nodiscard]] Engine::Render::Backend::SdlGpu::SdlGpuRenderDevice& RenderDevice();
    [[nodiscard]] Engine::Render::Renderer& Renderer();
    [[nodiscard]] ObjectFpsPresentation& Presentation();
    [[nodiscard]] ObjectFpsRuntimeClient& Client();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace fps
