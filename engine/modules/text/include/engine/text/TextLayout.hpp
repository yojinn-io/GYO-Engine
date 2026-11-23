#pragma once

#include <cstdint>
#include <vector>

#include "engine/text/TextStyle.hpp"

// A laid-out text (TextContext::Layout): plain data, readable from any number
// of threads. Pixels, y down, the origin at the top left of the layout box.
// Every value is a multiple of 1/64 px (the layout computes in 26.6 fixed
// point), so it is the same on every platform (Design Invariant TX-8).
namespace GYO::Engine::Text {

struct PositionedGlyph {
    FontId font;
    // The glyph id in that face.
    std::uint32_t glyph = 0;
    // Where the glyph is drawn: its origin on the baseline, the line's
    // alignment offset and the shaping offsets added.
    float x = 0.0f;
    float y = 0.0f;
    // The pen position before the glyph (no shaping offset), and its advance.
    float penX = 0.0f;
    float advance = 0.0f;
    float sizePx = 0.0f;
    // The cluster in the source text: [byteBegin, byteEnd). Clusters are in
    // the order of the text, do not overlap and cover every byte once
    // (Design Invariant TX-3). Glyphs of one cluster share the range.
    std::uint32_t byteBegin = 0;
    std::uint32_t byteEnd = 0;
    std::uint32_t span = 0;
    std::uint32_t line = 0;
    // False for what is laid out but never drawn: a line break, a tab, a
    // default-ignorable character. Rasterize skips it.
    bool visible = true;
};

struct LayoutLine {
    float top = 0.0f;
    float baseline = 0.0f;
    float height = 0.0f;
    // Without the trailing spaces and line break.
    float width = 0.0f;
    // The alignment offset (already in every glyph's x and penX).
    float x = 0.0f;
    std::uint32_t glyphBegin = 0;
    std::uint32_t glyphEnd = 0;
    std::uint32_t byteBegin = 0;
    std::uint32_t byteEnd = 0;
    // The line ends with a mandatory break (a line feed and the like), not
    // because the next word did not fit.
    bool hardBreak = false;
};

struct TextLayout {
    // In the order of the text.
    std::vector<PositionedGlyph> glyphs;
    // At least one line, even for an empty text.
    std::vector<LayoutLine> lines;
    // Every grapheme boundary as a byte offset, 0 and the text's length
    // included, ascending: the positions a caret may take.
    std::vector<std::uint32_t> graphemeBoundaries;
    // The widest line.
    float width = 0.0f;
    float height = 0.0f;
    float firstBaseline = 0.0f;
    std::uint32_t textBytes = 0;
};

} // namespace GYO::Engine::Text
