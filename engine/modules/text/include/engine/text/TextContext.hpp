#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "engine/text/FontData.hpp"
#include "engine/text/GlyphAtlas.hpp"
#include "engine/text/ShapedText.hpp"
#include "engine/text/TextFailure.hpp"
#include "engine/text/TextLayout.hpp"
#include "engine/text/TextStyle.hpp"

// The text Module's working object: the fonts added to it, shaping (HarfBuzz),
// line breaking and layout, and rasterizing glyphs into a GlyphAtlas
// (FreeType). The caller owns it (a Resource of the World that lays out text,
// like Bevy's FontCx): one user at a time, from any thread; no global state
// (Design Invariant TX-7).
namespace GYO::Engine::Text {

namespace Detail {
struct ContextState;
} // namespace Detail

struct FontInfo {
    std::string familyName;
    std::string styleName;
    std::uint32_t unitsPerEm = 0;
    std::uint32_t glyphCount = 0;
};

struct RasterResult {
    // One per glyph of the layout, in the same order.
    std::vector<GlyphPlacement> placements;
    // Glyphs drawn into the atlas by this call.
    std::uint32_t newGlyphs = 0;
    // Glyphs of the layout larger than a page.
    std::uint32_t tooLarge = 0;
};

class TextContext final {
public:
    TextContext();
    ~TextContext();
    TextContext(TextContext&&) noexcept;
    TextContext& operator=(TextContext&&) noexcept;
    TextContext(const TextContext&) = delete;
    TextContext& operator=(const TextContext&) = delete;

    // Opens face faceIndex of the font. The FontData is kept (shared).
    [[nodiscard]] TextResult<FontId> AddFont(const FontData& data, std::uint32_t faceIndex);
    [[nodiscard]] FontInfo Info(FontId font) const;
    [[nodiscard]] std::uint32_t FontCount() const noexcept;

    // Shapes a UTF-8 text. The spans cover it in order; every font of every
    // span belongs to this context. A maximal ill-formed UTF-8 subsequence is
    // shaped as U+FFFD and keeps its bytes as its cluster. The options'
    // language and strictness apply to the whole text.
    [[nodiscard]] ShapedText Shape(std::string_view utf8, std::span<const TextSpan> spans,
                                   const ShapeOptions& options = {});
    // Breaks a shaped text into lines and places its glyphs. The text must
    // come from this context's Shape.
    [[nodiscard]] TextLayout Layout(const ShapedText& shaped, const LayoutOptions& options);
    // Draws the layout's visible glyphs that are not in the atlas yet. An
    // atlas serves one context: the first Rasterize binds it.
    [[nodiscard]] RasterResult Rasterize(const TextLayout& layout, GlyphAtlas& atlas);

private:
    std::unique_ptr<Detail::ContextState> state_;
};

} // namespace GYO::Engine::Text
