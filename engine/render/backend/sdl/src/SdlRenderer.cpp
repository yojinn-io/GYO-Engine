#include "engine/render/backend/sdl/SdlRenderer.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

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

struct SdlRenderer::Impl final {
    SDL_Renderer* renderer{};
};

Base::Result<std::unique_ptr<SdlRenderer>, SdlRendererError>
SdlRenderer::Create(
    Platform::Sdl::SdlPlatform& platform,
    const SdlRendererOptions& options) {

    SDL_Renderer* renderer = SDL_CreateRenderer(Platform::Sdl::NativeWindow(platform), nullptr);
    if (renderer == nullptr) {
        return Base::Err(MakeSdlError(
            SdlRendererErrorCode::CreationFailed,
            "SdlRenderer: SDL renderer creation failed"));
    }

    // VSync availability depends on the selected SDL renderer driver. It is a
    // preference for this minimal backend, not a creation requirement.
    if (options.vsync) {
        static_cast<void>(SDL_SetRenderVSync(renderer, 1));
    }

    auto impl = std::make_unique<Impl>();
    impl->renderer = renderer;
    return std::unique_ptr<SdlRenderer>(new SdlRenderer(std::move(impl)));
}

SdlRenderer::SdlRenderer(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

SdlRenderer::~SdlRenderer() {
    if (impl_->renderer != nullptr) {
        SDL_DestroyRenderer(impl_->renderer);
        impl_->renderer = nullptr;
    }
}

Base::Result<void, SdlRendererError> SdlRenderer::Clear(Color color) {

    if (!SDL_SetRenderDrawColor(
            impl_->renderer, color.red, color.green, color.blue, color.alpha)) {
        return Base::Err(MakeSdlError(
            SdlRendererErrorCode::ClearFailed,
            "SdlRenderer: setting the clear color failed"));
    }

    if (!SDL_RenderClear(impl_->renderer)) {
        return Base::Err(MakeSdlError(
            SdlRendererErrorCode::ClearFailed,
            "SdlRenderer: clearing the render target failed"));
    }

    return {};
}

Base::Result<void, SdlRendererError> SdlRenderer::Present() {

    if (!SDL_RenderPresent(impl_->renderer)) {
        return Base::Err(MakeSdlError(
            SdlRendererErrorCode::PresentFailed,
            "SdlRenderer: presenting the render target failed"));
    }

    return {};
}

SDL_Renderer* RendererNativeAccess::Renderer(const SdlRenderer& renderer) noexcept {
    return renderer.impl_->renderer;
}

} // namespace Engine::Render::Backend::Sdl
