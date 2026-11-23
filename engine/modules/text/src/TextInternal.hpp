#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "adapters/freetype/FreeTypeRaster.hpp"
#include "adapters/harfbuzz/HarfBuzzShaping.hpp"
#include "adapters/libunibreak/Segmentation.hpp"
#include "engine/text/FontData.hpp"
#include "engine/text/GlyphAtlas.hpp"
#include "engine/text/TextContext.hpp"

// The text Module's internal data. Everything here uses only the adapters'
// interfaces (Design Invariant TX-1).
namespace GYO::Engine::Text::Detail {

struct FontBytes {
    std::vector<std::byte> bytes;
    std::uint32_t faceCount = 0;
};

// A context's identity: ShapedText and GlyphAtlas keep a weak reference and
// compare owners, never addresses (a context moves; an address is reused).
struct ContextIdentity {};

struct FontEntry {
    FontData data;
    std::unique_ptr<HarfBuzzFace> face;
    std::uint32_t freeTypeFace = 0;
    FontInfo info;
};

struct ContextState {
    std::shared_ptr<ContextIdentity> identity = std::make_shared<ContextIdentity>();
    FreeTypeLibrary freeType;
    HarfBuzzShaper shaper;
    std::vector<FontEntry> fonts;
};

struct StoredSpan {
    std::vector<FontId> fonts;
    std::int32_t size26 = 0;
    std::int32_t lineHeight26 = 0;
    std::uint32_t cpBegin = 0;
    std::uint32_t cpEnd = 0;
    // The primary font's metrics (an empty line uses them).
    LineMetrics metrics;
};

struct Run {
    std::uint32_t span = 0;
    FontId font;
    ScriptCode script = 0;
    std::uint32_t cpBegin = 0;
    std::uint32_t cpEnd = 0;
    LineMetrics metrics;
};

struct Glyph {
    std::uint32_t glyph = 0;
    std::uint32_t cpBegin = 0;
    std::uint32_t cpEnd = 0;
    std::int32_t advance = 0;
    std::int32_t xOffset = 0;
    std::int32_t yOffset = 0;
    std::uint32_t run = 0;
    bool unsafeToBreak = false;
    bool visible = true;
};

struct ShapedData {
    std::weak_ptr<ContextIdentity> owner;
    // ShapeOptions::language, kept for shaping again at a line break.
    std::string language;
    // The text's code points (ill-formed UTF-8 as U+FFFD) and the byte offset
    // of each (one more entry: the text's length).
    std::vector<char32_t> codePoints;
    std::vector<std::uint32_t> offsets;
    // What HarfBuzz is given: line breaks and tabs as U+0020.
    std::vector<char32_t> shapingCodePoints;
    std::vector<LineBreakAfter> lineBreaks;
    std::vector<std::uint8_t> graphemeEnds;
    std::vector<StoredSpan> spans;
    std::vector<Run> runs;
    std::vector<Glyph> glyphs;
};

struct GlyphKey {
    std::uint32_t font = 0;
    std::uint32_t glyph = 0;
    std::int32_t size26 = 0;
    // Light hinting with grey anti-aliasing: the only way glyphs are drawn now.
    std::uint8_t mode = 0;

    [[nodiscard]] friend bool operator<(const GlyphKey& a, const GlyphKey& b) noexcept {
        if (a.font != b.font) return a.font < b.font;
        if (a.glyph != b.glyph) return a.glyph < b.glyph;
        if (a.size26 != b.size26) return a.size26 < b.size26;
        return a.mode < b.mode;
    }
};

// Mandatory line breaks and the tab: shaped as spaces, never drawn, and hung
// at the end of a line.
[[nodiscard]] bool IsLineBreakOrTab(char32_t cp) noexcept;
// Characters whose width does not count at the end of a line.
[[nodiscard]] bool IsHangingSpace(char32_t cp) noexcept;

[[nodiscard]] std::int32_t ToFixed26(float value);
[[nodiscard]] constexpr float FromFixed26(const std::int64_t value) noexcept {
    return static_cast<float>(value) / 64.0f;
}

} // namespace GYO::Engine::Text::Detail
