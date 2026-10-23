#pragma once

#include <filesystem>

#include "engine/base/Result.hpp"
#include "engine/render/RenderError.hpp"
#include "engine/render/Renderer.hpp"

namespace Engine::Render::Backend::SdlGpu {

// Writes a scene capture (RGBA8, rows top to bottom) as a BMP file. Any thread;
// needs no platform initialization. The path is passed to the file system as
// UTF-8 on every platform. A capture whose pixel count does not match its size
// is InvalidArgument; a failed write is FileWriteFailed. The bytes follow SDL's
// BMP writer (SDL_HINT_BMP_SAVE_LEGACY_FORMAT changes them).
[[nodiscard]] Base::Result<void, RenderError> SaveCaptureBmp(const SceneCapture& capture,
                                                             const std::filesystem::path& path);

} // namespace Engine::Render::Backend::SdlGpu
