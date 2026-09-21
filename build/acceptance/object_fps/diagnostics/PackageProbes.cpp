#include "ObjectFpsDiagnostics.hpp"

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
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
