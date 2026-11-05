#include "engine/render/backend/sdl/SdlRenderer.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

#include <atomic>
#include <climits>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Engine::Render::Backend::Sdl {

namespace {

SdlRendererError MakeSdlError(
    SdlRendererErrorCode code,
    std::string message) {
    return SdlRendererError::Make(code, std::move(message), SDL_GetError());
}

SdlRendererError MakeError(SdlRendererErrorCode code, std::string message) {
    return SdlRendererError::Make(code, std::move(message));
}

// Unique across renderers, so a handle of one renderer is no texture of another.
std::atomic<std::uint32_t> nextGeneration{1};

} // namespace

struct SdlRenderer::Impl final {
    struct TextureSlot final {
        SDL_Texture* texture{};
        std::uint32_t generation{};
        std::uint32_t width{}, height{};
    };
    SDL_Renderer* renderer{};
    std::vector<TextureSlot> textures;
    std::vector<std::uint32_t> freeTextures;

    [[nodiscard]] TextureSlot* Find(SdlTextureHandle handle) noexcept {
        if (!handle || handle.index >= textures.size()) return nullptr;
        TextureSlot& slot = textures[handle.index];
        return slot.texture != nullptr && slot.generation == handle.generation ? &slot : nullptr;
    }
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

Base::Result<SdlTextureHandle, SdlRendererError> SdlRenderer::CreateStreamingTexture(
    std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || width > INT_MAX || height > INT_MAX) {
        return Base::Err(MakeError(SdlRendererErrorCode::InvalidArgument,
            "SdlRenderer: a texture needs a nonzero size SDL can hold"));
    }
    // The slot first, so that nothing can fail once the SDL texture exists.
    if (impl_->freeTextures.empty()) impl_->textures.reserve(impl_->textures.size() + 1);
    SDL_Texture* texture = SDL_CreateTexture(impl_->renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, static_cast<int>(width), static_cast<int>(height));
    if (texture == nullptr) {
        return Base::Err(MakeSdlError(SdlRendererErrorCode::TextureFailed,
            "SdlRenderer: texture creation failed"));
    }
    std::uint32_t generation = nextGeneration.fetch_add(1, std::memory_order_relaxed);
    if (generation == 0) generation = nextGeneration.fetch_add(1, std::memory_order_relaxed);
    std::uint32_t index{};
    if (impl_->freeTextures.empty()) {
        index = static_cast<std::uint32_t>(impl_->textures.size());
        impl_->textures.emplace_back();
    } else {
        index = impl_->freeTextures.back();
        impl_->freeTextures.pop_back();
    }
    impl_->textures[index] = {texture, generation, width, height};
    return SdlTextureHandle{index, generation};
}

Base::Result<void, SdlRendererError> SdlRenderer::UploadTexture(
    SdlTextureHandle texture, std::span<const std::byte> bytes, std::uint32_t rowPitch) {
    Impl::TextureSlot* slot = impl_->Find(texture);
    if (slot == nullptr) {
        return Base::Err(MakeError(SdlRendererErrorCode::InvalidArgument,
            "SdlRenderer: not a live texture of this renderer"));
    }
    const std::uint64_t row = static_cast<std::uint64_t>(slot->width) * 4U;
    const std::uint64_t needed = static_cast<std::uint64_t>(rowPitch) * (slot->height - 1U) + row;
    if (rowPitch < row || rowPitch > INT_MAX || bytes.size() < needed) {
        return Base::Err(MakeError(SdlRendererErrorCode::InvalidArgument,
            "SdlRenderer: the pixels do not fill the texture"));
    }
    if (!SDL_UpdateTexture(slot->texture, nullptr, bytes.data(), static_cast<int>(rowPitch))) {
        return Base::Err(MakeSdlError(SdlRendererErrorCode::TextureFailed,
            "SdlRenderer: texture upload failed"));
    }
    return {};
}

Base::Result<void, SdlRendererError> SdlRenderer::ReleaseTexture(SdlTextureHandle texture) {
    Impl::TextureSlot* slot = impl_->Find(texture);
    if (slot == nullptr) {
        return Base::Err(MakeError(SdlRendererErrorCode::InvalidArgument,
            "SdlRenderer: not a live texture of this renderer"));
    }
    SDL_DestroyTexture(slot->texture);
    *slot = {};
    impl_->freeTextures.push_back(texture.index);
    return {};
}

SDL_Renderer* RendererNativeAccess::Renderer(const SdlRenderer& renderer) noexcept {
    return renderer.impl_->renderer;
}

SDL_Texture* RendererNativeAccess::Texture(const SdlRenderer& renderer, SdlTextureHandle texture) noexcept {
    const SdlRenderer::Impl::TextureSlot* slot = renderer.impl_->Find(texture);
    return slot != nullptr ? slot->texture : nullptr;
}

} // namespace Engine::Render::Backend::Sdl
