#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

// The text Module's failures (E2: this font was not taken, everything else is
// fine). The values are numbered TX-E1 ... in docs/architecture/target/v2/text.zh-Hant.md.
namespace GYO::Engine::Text {

enum class TextFailureCode : std::uint8_t {
    // Not an sfnt file or collection (TrueType, OpenType/CFF, .ttc, .otc);
    // WOFF, WOFF2 and Type 1 are not taken. Rule 1 of the "font" Data Contract.
    NotSfnt = 1,
    // A collection with no face. Rule 2.
    EmptyCollection = 2,
    // A face HarfBuzz or FreeType cannot open. Rule 2.
    FaceUnreadable = 3,
    // A face with no glyph. Rule 2.
    NoGlyphs = 4,
    // A face without a Unicode character map. Rule 2.
    NoUnicodeCmap = 5,
    // AddFont: the face index is not below the font's FaceCount().
    FaceIndexOutOfRange = 6,
    // Memory ran out while a font was opened.
    OutOfMemory = 7,
};

[[nodiscard]] constexpr const char* ToString(const TextFailureCode code) noexcept {
    switch (code) {
    case TextFailureCode::NotSfnt: return "NotSfnt";
    case TextFailureCode::EmptyCollection: return "EmptyCollection";
    case TextFailureCode::FaceUnreadable: return "FaceUnreadable";
    case TextFailureCode::NoGlyphs: return "NoGlyphs";
    case TextFailureCode::NoUnicodeCmap: return "NoUnicodeCmap";
    case TextFailureCode::FaceIndexOutOfRange: return "FaceIndexOutOfRange";
    case TextFailureCode::OutOfMemory: return "OutOfMemory";
    }
    return "Unknown";
}

using TextFailure = Base::Error<TextFailureCode>;

template <class T>
using TextResult = Base::Result<T, TextFailure>;

} // namespace GYO::Engine::Text
