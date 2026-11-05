#pragma once

// The native layer of the SDL renderer backend (docs/architecture/platform-sdl.md).
// Linking GYO::RenderBackendSDLNative declares direct use of SDL.

#include <SDL3/SDL_render.h>

#include "engine/render/backend/sdl/SdlRenderer.hpp"

namespace Engine::Render::Backend::Sdl {

class RendererNativeAccess final {
public:
    [[nodiscard]] static SDL_Renderer* Renderer(const SdlRenderer& renderer) noexcept;
    [[nodiscard]] static SDL_Texture* Texture(const SdlRenderer& renderer, SdlTextureHandle texture) noexcept;
};

[[nodiscard]] inline SDL_Renderer* NativeRenderer(const SdlRenderer& renderer) noexcept {
    return RendererNativeAccess::Renderer(renderer);
}

// The SDL texture of a live texture of renderer; nullptr otherwise (released,
// never created, or another renderer's).
[[nodiscard]] inline SDL_Texture* NativeTexture(const SdlRenderer& renderer, SdlTextureHandle texture) noexcept {
    return RendererNativeAccess::Texture(renderer, texture);
}

} // namespace Engine::Render::Backend::Sdl
