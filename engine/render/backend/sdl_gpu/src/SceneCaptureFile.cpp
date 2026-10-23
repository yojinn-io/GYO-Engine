#include "engine/render/backend/sdl_gpu/SceneCaptureFile.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <string>

namespace Engine::Render::Backend::SdlGpu {

Base::Result<void, RenderError> SaveCaptureBmp(const SceneCapture& capture, const std::filesystem::path& path) {
    if (capture.width == 0 || capture.height == 0 ||
        capture.rgba8.size() != static_cast<std::size_t>(capture.width) * capture.height * 4) {
        return Base::Err(RenderError::Make(RenderErrorCode::InvalidArgument,
                                           "SaveCaptureBmp: the pixels do not match the capture size"));
    }
    // SDL reads the pixels only; the surface does not own them.
    SDL_Surface* surface = SDL_CreateSurfaceFrom(static_cast<int>(capture.width), static_cast<int>(capture.height),
                                                 SDL_PIXELFORMAT_RGBA32,
                                                 const_cast<std::uint8_t*>(capture.rgba8.data()),
                                                 static_cast<int>(capture.width * 4));
    if (surface == nullptr) {
        return Base::Err(RenderError::Make(RenderErrorCode::FileWriteFailed,
                                           "SaveCaptureBmp: the capture surface could not be made", SDL_GetError()));
    }
    const std::u8string utf8 = path.u8string();
    const bool saved = SDL_SaveBMP(surface, reinterpret_cast<const char*>(utf8.c_str()));
    SDL_DestroySurface(surface);
    if (!saved) {
        return Base::Err(RenderError::Make(RenderErrorCode::FileWriteFailed,
                                           "SaveCaptureBmp: writing the file failed", SDL_GetError()));
    }
    return {};
}

} // namespace Engine::Render::Backend::SdlGpu
