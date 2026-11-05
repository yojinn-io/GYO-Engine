#include "gyo/framework/PreviewTexture.hpp"

#include "engine/base/Error.hpp"

namespace Gyo::Tools::Framework {

Result<void, PreviewError> UploadFrame(Engine::Render::Backend::Sdl::SdlRenderer& renderer, PreviewTexture& texture,
                                       const Engine::Render::TextureReadback& frame) {
    // The window's streaming texture holds RGBA8 in sRGB, as the offscreen device reads its frame back.
    if (frame.format != Engine::Render::TextureFormat::Rgba8SRgb) {
        return Engine::Base::Err(PreviewError::Make(PreviewErrorCode::UploadFailed,
                                                    "the frame is not RGBA8 in sRGB", "the texture is not changed"));
    }
    if (texture.handle && (texture.width != frame.width || texture.height != frame.height)) {
        static_cast<void>(renderer.ReleaseTexture(texture.handle));
        texture = {};
    }
    if (!texture.handle) {
        auto created = renderer.CreateStreamingTexture(frame.width, frame.height);
        if (!created) {
            return Engine::Base::Err(PreviewError::Make(PreviewErrorCode::UploadFailed, "cannot create the preview texture",
                                                        Engine::Base::CauseDetail(created.error())));
        }
        texture = {created.value(), frame.width, frame.height};
    }
    auto uploaded = renderer.UploadTexture(texture.handle, frame.bytes, frame.rowPitch);
    if (!uploaded) {
        return Engine::Base::Err(PreviewError::Make(PreviewErrorCode::UploadFailed, "cannot upload the preview",
                                                    Engine::Base::CauseDetail(uploaded.error())));
    }
    return {};
}

} // namespace Gyo::Tools::Framework
