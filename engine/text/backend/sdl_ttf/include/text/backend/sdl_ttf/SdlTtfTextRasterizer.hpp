#pragma once

#include <memory>

#include "text/ITextRasterizer.hpp"

namespace Engine::Text::Backend::SdlTtf {

// Optional SDL_ttf implementation. SDL_ttf and SDL surface/font types remain
// private to the implementation and never cross this public boundary.
class SdlTtfTextRasterizer final : public ITextRasterizer {
public:
    [[nodiscard]] static Base::Result<
        std::unique_ptr<SdlTtfTextRasterizer>,
        TextError> Create();

    ~SdlTtfTextRasterizer() override;

    SdlTtfTextRasterizer(const SdlTtfTextRasterizer&) = delete;
    SdlTtfTextRasterizer& operator=(const SdlTtfTextRasterizer&) = delete;
    SdlTtfTextRasterizer(SdlTtfTextRasterizer&&) = delete;
    SdlTtfTextRasterizer& operator=(SdlTtfTextRasterizer&&) = delete;

    [[nodiscard]] Base::Result<TextBitmap, TextError> Rasterize(
        std::span<const std::byte> encodedFont,
        const TextRasterRequest& request) override;

private:
    SdlTtfTextRasterizer() noexcept = default;
};

} // namespace Engine::Text::Backend::SdlTtf
