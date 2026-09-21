#include "render/backend/sdl/SdlRenderer.hpp"

#include <string>
#include <utility>

namespace Engine::Render::Backend::Sdl {

namespace {

SdlRendererError MakeSdlError(
    SdlRendererErrorCode code,
    std::string message) {
    return SdlRendererError::Make(code, std::move(message), SDL_GetError());
}

} // namespace

Base::Result<std::unique_ptr<SdlRenderer>, SdlRendererError>
SdlRenderer::Create(
    Platform::Sdl::SdlPlatform& platform,
    const SdlRendererOptions& options) {
    using Result = Base::Result<std::unique_ptr<SdlRenderer>, SdlRendererError>;

    SDL_Renderer* renderer = SDL_CreateRenderer(platform.NativeWindow(), nullptr);
    if (renderer == nullptr) {
        return Result::Err(MakeSdlError(
            SdlRendererErrorCode::CreationFailed,
            "SdlRenderer: SDL renderer creation failed"));
    }

    // VSync availability depends on the selected SDL renderer driver. It is a
    // preference for this minimal backend, not a creation requirement.
    if (options.vsync) {
        static_cast<void>(SDL_SetRenderVSync(renderer, 1));
    }

    return Result::Ok(std::unique_ptr<SdlRenderer>(new SdlRenderer(renderer)));
}

SdlRenderer::SdlRenderer(SDL_Renderer* renderer) noexcept
    : renderer_(renderer) {}

SdlRenderer::~SdlRenderer() {
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
}

Base::Result<void, SdlRendererError> SdlRenderer::Clear(Color color) {
    using Result = Base::Result<void, SdlRendererError>;

    if (!SDL_SetRenderDrawColor(
            renderer_, color.red, color.green, color.blue, color.alpha)) {
        return Result::Err(MakeSdlError(
            SdlRendererErrorCode::ClearFailed,
            "SdlRenderer: setting the clear color failed"));
    }

    if (!SDL_RenderClear(renderer_)) {
        return Result::Err(MakeSdlError(
            SdlRendererErrorCode::ClearFailed,
            "SdlRenderer: clearing the render target failed"));
    }

    return Result::Ok();
}

Base::Result<void, SdlRendererError> SdlRenderer::Present() {
    using Result = Base::Result<void, SdlRendererError>;

    if (!SDL_RenderPresent(renderer_)) {
        return Result::Err(MakeSdlError(
            SdlRendererErrorCode::PresentFailed,
            "SdlRenderer: presenting the render target failed"));
    }

    return Result::Ok();
}

SDL_Renderer* SdlRenderer::NativeRenderer() const noexcept {
    return renderer_;
}

} // namespace Engine::Render::Backend::Sdl
