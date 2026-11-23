#pragma once

#include <cstdint>
#include <vector>

#include "engine/text/TextLayout.hpp"

// Queries on a TextLayout for a text box: where a caret is drawn, which caret
// a point hits, and the rectangles of a range (a selection, the underline of
// an IME composition). They read only the layout's clusters, grapheme
// boundaries and lines (Design Invariant TX-3); no font is touched.
namespace GYO::Engine::Text {

// At a soft line break one byte offset is both the end of a line (Upstream)
// and the start of the next (Downstream).
enum class CaretAffinity : std::uint8_t { Downstream = 0, Upstream = 1 };

struct Caret {
    // A grapheme boundary (TextLayout::graphemeBoundaries).
    std::uint32_t byteOffset = 0;
    CaretAffinity affinity = CaretAffinity::Downstream;

    [[nodiscard]] friend bool operator==(const Caret&, const Caret&) = default;
};

struct CaretRect {
    float x = 0.0f;
    float top = 0.0f;
    float height = 0.0f;
    std::uint32_t line = 0;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    [[nodiscard]] friend bool operator==(const Rect&, const Rect&) = default;
};

// A caret not on a grapheme boundary of the layout is a Programmer Error.
[[nodiscard]] CaretRect CaretAt(const TextLayout& layout, Caret caret);

// The caret nearest to the point, on the line the point's y falls on (the
// first line above the text, the last below it).
[[nodiscard]] Caret HitTest(const TextLayout& layout, float x, float y);

// One rectangle per line the range [byteBegin, byteEnd) touches, appended to
// out. Both ends are grapheme boundaries.
void RangeRects(const TextLayout& layout, std::uint32_t byteBegin, std::uint32_t byteEnd, std::vector<Rect>& out);

} // namespace GYO::Engine::Text
