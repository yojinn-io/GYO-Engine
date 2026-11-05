#pragma once

#include "gyo/framework/FrameworkError.hpp"

#include "engine/render/RenderDeviceTypes.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"

#include <cstdint>

namespace Gyo::Tools::Framework {

// The texture of the window's SDL renderer that shows a preview, and its size.
struct PreviewTexture final {
    Engine::Render::Backend::Sdl::SdlTextureHandle handle;
    std::uint32_t width{}, height{};
};

// Puts frame into texture: a texture of another size (or none) is released
// and a new one of the frame's size made, then the frame's bytes are uploaded
// as they were read back (UploadFailed when the renderer refuses, or when the
// frame is not Rgba8SRgb, the texture then unchanged). Call it
// before the texture is drawn in a frame, never after. The caller releases the
// texture (SdlRenderer::ReleaseTexture) when it is done with it.
[[nodiscard]] Result<void, PreviewError> UploadFrame(Engine::Render::Backend::Sdl::SdlRenderer& renderer,
                                                     PreviewTexture& texture,
                                                     const Engine::Render::TextureReadback& frame);

} // namespace Gyo::Tools::Framework
