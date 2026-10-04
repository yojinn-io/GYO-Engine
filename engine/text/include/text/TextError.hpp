#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"

namespace Engine::Text {

// Zero is not a valid code; the numeric values are not a data contract.
enum class TextErrorCode : std::uint8_t {
    InvalidArgument = 1,
    InitializationFailed,
    FontOpenFailed,
    RasterizationFailed,
    PixelConversionFailed,
    SizeOverflow,
};

[[nodiscard]] constexpr const char* ToString(const TextErrorCode code) noexcept {
    switch (code) {
    case TextErrorCode::InvalidArgument: return "InvalidArgument";
    case TextErrorCode::InitializationFailed: return "InitializationFailed";
    case TextErrorCode::FontOpenFailed: return "FontOpenFailed";
    case TextErrorCode::RasterizationFailed: return "RasterizationFailed";
    case TextErrorCode::PixelConversionFailed: return "PixelConversionFailed";
    case TextErrorCode::SizeOverflow: return "SizeOverflow";
    }
    return "Unknown";
}

using TextError = Base::Error<TextErrorCode>;
static_assert(Base::CodedError<TextError>);

} // namespace Engine::Text
