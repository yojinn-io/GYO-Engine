#include "text/backend/sdl_ttf/SdlTtfTextRasterizer.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace Engine::Text::Backend::SdlTtf {
namespace {

struct IoCloser final {
    void operator()(SDL_IOStream* stream) const noexcept {
        if (stream != nullptr) {
            static_cast<void>(SDL_CloseIO(stream));
        }
    }
};

struct FontCloser final {
    void operator()(TTF_Font* font) const noexcept {
        TTF_CloseFont(font);
    }
};

struct SurfaceCloser final {
    void operator()(SDL_Surface* surface) const noexcept {
        SDL_DestroySurface(surface);
    }
};

using IoPtr = std::unique_ptr<SDL_IOStream, IoCloser>;
using FontPtr = std::unique_ptr<TTF_Font, FontCloser>;
using SurfacePtr = std::unique_ptr<SDL_Surface, SurfaceCloser>;

[[nodiscard]] TextError MakeError(
    const TextErrorCode code,
    std::string message) {
    const char* detail = SDL_GetError();
    return TextError::Make(
        code,
        std::move(message),
        detail != nullptr ? std::string(detail) : std::string{});
}

} // namespace

Base::Result<std::unique_ptr<SdlTtfTextRasterizer>, TextError>
SdlTtfTextRasterizer::Create() {
    using Result = Base::Result<
        std::unique_ptr<SdlTtfTextRasterizer>,
        TextError>;

    SDL_ClearError();
    if (!TTF_Init()) {
        return Result::Err(MakeError(
            TextErrorCode::InitializationFailed,
            "SDL_ttf text rasterizer failed to initialize SDL_ttf"));
    }

    return Result::Ok(std::unique_ptr<SdlTtfTextRasterizer>(
        new SdlTtfTextRasterizer()));
}

SdlTtfTextRasterizer::~SdlTtfTextRasterizer() {
    TTF_Quit();
}

Base::Result<TextBitmap, TextError> SdlTtfTextRasterizer::Rasterize(
    const std::span<const std::byte> encodedFont,
    const TextRasterRequest& request) {
    using Result = Base::Result<TextBitmap, TextError>;

    if (encodedFont.empty()) {
        return Result::Err(TextError::Make(
            TextErrorCode::InvalidArgument,
            "SDL_ttf text rasterizer requires encoded font bytes"));
    }
    if (request.utf8.empty()) {
        return Result::Err(TextError::Make(
            TextErrorCode::InvalidArgument,
            "SDL_ttf text rasterizer requires non-empty UTF-8 text"));
    }
    if (!std::isfinite(request.pointSize) || request.pointSize <= 0.0F) {
        return Result::Err(TextError::Make(
            TextErrorCode::InvalidArgument,
            "SDL_ttf text rasterizer requires a finite positive point size"));
    }

    SDL_ClearError();
    IoPtr stream(SDL_IOFromConstMem(encodedFont.data(), encodedFont.size()));
    if (!stream) {
        return Result::Err(MakeError(
            TextErrorCode::FontOpenFailed,
            "SDL_ttf text rasterizer could not create a font memory stream"));
    }

    // Keep the stream alive until after the font is closed. Ownership remains
    // explicit instead of relying on closeio behavior on failed font creation.
    FontPtr font(TTF_OpenFontIO(stream.get(), false, request.pointSize));
    if (!font) {
        return Result::Err(MakeError(
            TextErrorCode::FontOpenFailed,
            "SDL_ttf text rasterizer could not open the encoded font"));
    }

    constexpr SDL_Color white{255, 255, 255, 255};
    SurfacePtr rendered(TTF_RenderText_Blended(
        font.get(),
        request.utf8.data(),
        request.utf8.size(),
        white));
    if (!rendered) {
        return Result::Err(MakeError(
            TextErrorCode::RasterizationFailed,
            "SDL_ttf text rasterizer could not rasterize the UTF-8 text"));
    }

    SurfacePtr rgba(SDL_ConvertSurface(rendered.get(), SDL_PIXELFORMAT_RGBA32));
    if (!rgba) {
        return Result::Err(MakeError(
            TextErrorCode::PixelConversionFailed,
            "SDL_ttf text rasterizer could not convert glyph pixels to RGBA8"));
    }
    if (rgba->w <= 0 || rgba->h <= 0 || rgba->pixels == nullptr || rgba->pitch < 0) {
        return Result::Err(TextError::Make(
            TextErrorCode::RasterizationFailed,
            "SDL_ttf text rasterizer produced an empty or invalid surface"));
    }

    const auto width = static_cast<std::size_t>(rgba->w);
    const auto height = static_cast<std::size_t>(rgba->h);
    constexpr std::size_t bytesPerPixel = 4;
    if (width > (std::numeric_limits<std::uint32_t>::max)() ||
        height > (std::numeric_limits<std::uint32_t>::max)() ||
        width > (std::numeric_limits<std::size_t>::max)() / bytesPerPixel) {
        return Result::Err(TextError::Make(
            TextErrorCode::SizeOverflow,
            "SDL_ttf text rasterizer output dimensions overflow RGBA8 storage"));
    }

    const std::size_t rowBytes = width * bytesPerPixel;
    if (rowBytes > (std::numeric_limits<std::uint32_t>::max)() ||
        static_cast<std::size_t>(rgba->pitch) < rowBytes ||
        height > (std::numeric_limits<std::size_t>::max)() / rowBytes) {
        return Result::Err(TextError::Make(
            TextErrorCode::SizeOverflow,
            "SDL_ttf text rasterizer output pitch or byte size is invalid"));
    }

    TextBitmap bitmap;
    bitmap.width = static_cast<std::uint32_t>(width);
    bitmap.height = static_cast<std::uint32_t>(height);
    bitmap.rowPitch = static_cast<std::uint32_t>(rowBytes);
    bitmap.rgba8.resize(rowBytes * height);

    const auto* source = static_cast<const std::byte*>(rgba->pixels);
    for (std::size_t row = 0; row < height; ++row) {
        std::copy_n(
            source + row * static_cast<std::size_t>(rgba->pitch),
            rowBytes,
            bitmap.rgba8.data() + row * rowBytes);
    }

    return Result::Ok(std::move(bitmap));
}

} // namespace Engine::Text::Backend::SdlTtf
