#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"

namespace Engine::Text {

enum class TextErrorCode : std::uint8_t {
    None = 0,
    InvalidArgument,
    InitializationFailed,
    FontOpenFailed,
    RasterizationFailed,
    PixelConversionFailed,
    SizeOverflow,
};

using TextError = Base::Error<TextErrorCode>;

} // namespace Engine::Text
