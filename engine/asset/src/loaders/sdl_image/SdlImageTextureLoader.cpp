#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "engine/asset/loaders/TextureAsset.hpp"

namespace Engine::Asset::Loaders::SdlImage {
namespace {

struct SurfaceDeleter final {
    void operator()(SDL_Surface* surface) const noexcept {
        SDL_DestroySurface(surface);
    }
};

using SurfacePtr = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

Base::Result<Core::AnyAsset, Loading::AssetError>
DecodeFailed(const Loading::LoadContext& context, std::string message) {
    const char* sdlError = SDL_GetError();
    if (sdlError != nullptr && *sdlError != '\0') {
        message += ": ";
        message += sdlError;
    }
    return Base::Result<Core::AnyAsset, Loading::AssetError>::Err(
        Loading::AssetError::Make(
            AssetErrorCode::DecodeFailed,
            std::move(message),
            context.resolvedPath));
}

} // namespace

AssetType SdlImageTextureLoader::GetType() const noexcept {
    return AssetType::Texture();
}

Base::Result<Core::AnyAsset, Loading::AssetError>
SdlImageTextureLoader::Load(const Base::ConstSpan<std::byte> bytes,
                            const Loading::LoadContext& context) {
    if (bytes.empty()) {
        return DecodeFailed(context, "SDL_image texture: input is empty");
    }

    SDL_ClearError();
    SDL_IOStream* io = SDL_IOFromConstMem(bytes.data(), bytes.size());
    if (io == nullptr) {
        return DecodeFailed(context, "SDL_image texture: cannot create memory stream");
    }

    // IMG_Load_IO owns and closes the stream when closeio is true.
    SurfacePtr decoded(IMG_Load_IO(io, true));
    if (!decoded) {
        return DecodeFailed(context, "SDL_image texture: unsupported or invalid image");
    }

    SurfacePtr rgba(SDL_ConvertSurface(decoded.get(), SDL_PIXELFORMAT_RGBA32));
    if (!rgba) {
        return DecodeFailed(context, "SDL_image texture: cannot convert to RGBA8");
    }
    if (rgba->w <= 0 || rgba->h <= 0 || rgba->pixels == nullptr) {
        return DecodeFailed(context, "SDL_image texture: decoded surface is empty");
    }

    const auto width = static_cast<std::size_t>(rgba->w);
    const auto height = static_cast<std::size_t>(rgba->h);
    constexpr std::size_t kBytesPerPixel = 4;
    if (width > (std::numeric_limits<std::size_t>::max)() / kBytesPerPixel) {
        return DecodeFailed(context, "SDL_image texture: row size overflows");
    }
    const std::size_t rowBytes = width * kBytesPerPixel;
    if (height > (std::numeric_limits<std::size_t>::max)() / rowBytes ||
        rgba->pitch < 0 || static_cast<std::size_t>(rgba->pitch) < rowBytes) {
        return DecodeFailed(context, "SDL_image texture: invalid surface pitch or size");
    }

    auto texture = std::make_shared<TextureAsset>();
    texture->width = static_cast<std::uint32_t>(rgba->w);
    texture->height = static_cast<std::uint32_t>(rgba->h);
    texture->rgba.resize(rowBytes * height);

    const auto* source = static_cast<const std::uint8_t*>(rgba->pixels);
    for (std::size_t row = 0; row < height; ++row) {
        std::copy_n(source + row * static_cast<std::size_t>(rgba->pitch),
                    rowBytes,
                    texture->rgba.data() + row * rowBytes);
    }

    return Base::Result<Core::AnyAsset, Loading::AssetError>::Ok(
        Core::AnyAsset::FromShared<TextureAsset>(std::move(texture)));
}

} // namespace Engine::Asset::Loaders::SdlImage
