#pragma once

#include <cstddef>
#include <span>

#include "engine/base/Result.hpp"
#include "text/TextError.hpp"
#include "text/TextTypes.hpp"

namespace Engine::Text {

// Infrastructure-neutral font/text seam. encodedFont is borrowed only for the
// duration of Rasterize(); the returned bitmap owns its pixels.
class ITextRasterizer {
public:
    virtual ~ITextRasterizer() = default;

    [[nodiscard]] virtual Base::Result<TextBitmap, TextError> Rasterize(
        std::span<const std::byte> encodedFont,
        const TextRasterRequest& request) = 0;
};

} // namespace Engine::Text
