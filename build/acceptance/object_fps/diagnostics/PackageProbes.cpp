#include "ObjectFpsDiagnostics.hpp"

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/RenderQueue.hpp"
#include "render/Renderer.hpp"
#include "render/ShaderLibrary.hpp"

#include <SDL3/SDL.h>

#include <string>

namespace {
template <class Error>
void LogError(const Error& error) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s%s%s", error.message.c_str(),
        error.detail.empty() ? "" : ": ", error.detail.c_str());
}
void LogError(const std::string& message) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
}
} // namespace

int ValidatePackage(const Engine::Asset::AssetCatalog& catalog, const Engine::Render::ShaderLibrary& shaders) {
    Engine::Asset::Loading::NativeFileAssetSource source;
    for (const auto* entry : catalog.Entries()) {
        const auto bytes = source.ReadAll(entry->resolvedPath);
        if (!bytes) { LogError(bytes.error()); return 1; }
        if (bytes.value().empty()) { LogError(std::string{"empty package asset: "} + entry->resolvedPath); return 1; }
    }
    SDL_Log("Package validation passed: assets=%zu, shader programs=%zu, formats=%u, bundle=%s",
        catalog.Entries().size(), shaders.ProgramIds().size(), shaders.CompleteFormats(), shaders.Version().c_str());
    return 0;
}

int RunShaderProbe(Engine::Platform::Sdl::SdlPlatform& platform, Engine::Render::Renderer& renderer) {
    namespace Render = Engine::Render;
    Render::RenderQueue queue;
    Render::SpriteSubmission left;
    left.destinationPixels = {16,16,64,64};
    left.material.tint = {1,0,0,1};
    left.layer = Render::CompositeLayer::Scene;
    auto right = left;
    right.destinationPixels.x = 112;
    right.material.shader = "game/object_fps/channel_swap";
    if (!queue.Submit(left) || !queue.Submit(right)) return 1;
    renderer.RequestSceneCapture();
    for (int attempt=0;attempt<180;++attempt) {
        if (platform.PumpEvents() == Engine::Runtime::RuntimeControl::Stop) return 1;
        const auto presented = renderer.Render(queue);
        if (!presented) { LogError(presented.error()); return 1; }
        const auto capture = renderer.TakeSceneCapture();
        if (!capture) { SDL_Delay(16); continue; }
        const auto sample = [&](std::uint32_t x, std::uint32_t y, std::size_t channel) {
            return capture->rgba8[(static_cast<std::size_t>(y) * capture->width + x) * 4 + channel];
        };
        if (capture->width < 176 || capture->height < 80 ||
            sample(48,48,0) < 240 || sample(48,48,2) > 10 ||
            sample(144,48,2) < 240 || sample(144,48,0) > 10) {
            LogError(std::string{"Game Shader smoke failed: expected red builtin and blue game quad"}); return 1;
        }
        SDL_Log("Game Shader smoke passed: builtin red, game/object_fps/channel_swap blue");
        return 0;
    }
    LogError(std::string{"Game Shader smoke could not acquire a frame"});
    return 1;
}

