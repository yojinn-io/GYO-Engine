#include "engine/text/Caret.hpp"

#include <algorithm>
#include <cmath>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Text {

namespace {

bool IsBoundary(const TextLayout& layout, const std::uint32_t offset) {
    return std::binary_search(layout.graphemeBoundaries.begin(), layout.graphemeBoundaries.end(), offset);
}

// The x of a caret at `offset` on `line`: the pen position at the start of the
// cluster, a share of the cluster's advance per grapheme inside a cluster
// (a ligature), the end of the line's last glyph at the line's end.
float XAt(const TextLayout& layout, const LayoutLine& line, const std::uint32_t offset) {
    if (line.glyphBegin == line.glyphEnd) {
        return line.x;
    }
    if (offset >= line.byteEnd) {
        const PositionedGlyph& last = layout.glyphs[line.glyphEnd - 1];
        return last.penX + last.advance;
    }
    const auto begin = layout.glyphs.begin() + line.glyphBegin;
    const auto end = layout.glyphs.begin() + line.glyphEnd;
    const auto first = std::partition_point(begin, end, [&](const PositionedGlyph& g) { return g.byteEnd <= offset; });
    GYO_ASSERT(first != end);
    if (offset <= first->byteBegin) {
        return first->penX;
    }
    float advance = 0.0f;
    for (auto it = first; it != end && it->byteBegin == first->byteBegin; ++it) {
        advance += it->advance;
    }
    const auto& bounds = layout.graphemeBoundaries;
    const auto inner = std::upper_bound(bounds.begin(), bounds.end(), first->byteBegin);
    const auto clusterEnd = std::lower_bound(inner, bounds.end(), first->byteEnd);
    const auto upToOffset = std::upper_bound(inner, clusterEnd, offset);
    const auto graphemes = static_cast<float>(clusterEnd - inner) + 1.0f;
    const auto passed = static_cast<float>(upToOffset - inner);
    return first->penX + advance * passed / graphemes;
}

std::size_t LineOf(const TextLayout& layout, const Caret caret) {
    const auto& lines = layout.lines;
    std::size_t l = 0;
    while (l < lines.size() && caret.byteOffset >= lines[l].byteEnd) {
        ++l;
    }
    if (l == lines.size()) {
        return lines.size() - 1;
    }
    if (l > 0 && caret.byteOffset == lines[l].byteBegin && !lines[l - 1].hardBreak &&
        caret.affinity == CaretAffinity::Upstream) {
        return l - 1;
    }
    return l;
}

} // namespace

CaretRect CaretAt(const TextLayout& layout, const Caret caret) {
    GYO_ASSERT(!layout.lines.empty());
    GYO_ASSERT(IsBoundary(layout, caret.byteOffset));
    const std::size_t l = LineOf(layout, caret);
    const LayoutLine& line = layout.lines[l];
    return CaretRect{XAt(layout, line, caret.byteOffset), line.top, line.height, static_cast<std::uint32_t>(l)};
}

Caret HitTest(const TextLayout& layout, const float x, const float y) {
    GYO_ASSERT(!layout.lines.empty());
    std::size_t l = 0;
    while (l + 1 < layout.lines.size() && y >= layout.lines[l].top + layout.lines[l].height) {
        ++l;
    }
    const LayoutLine& line = layout.lines[l];
    const bool last = l + 1 == layout.lines.size();
    // After a mandatory break the line's end belongs to the next line.
    const bool takesEnd = !(line.hardBreak && !last);
    const auto& bounds = layout.graphemeBoundaries;
    Caret best{line.byteBegin, CaretAffinity::Downstream};
    float bestDistance = std::fabs(XAt(layout, line, line.byteBegin) - x);
    for (auto it = std::upper_bound(bounds.begin(), bounds.end(), line.byteBegin);
         it != bounds.end() && (*it < line.byteEnd || (takesEnd && *it == line.byteEnd)); ++it) {
        const float distance = std::fabs(XAt(layout, line, *it) - x);
        if (distance < bestDistance) {
            bestDistance = distance;
            best.byteOffset = *it;
        }
    }
    if (best.byteOffset == line.byteEnd && !last && !line.hardBreak) {
        best.affinity = CaretAffinity::Upstream;
    }
    return best;
}

void RangeRects(const TextLayout& layout, const std::uint32_t byteBegin, const std::uint32_t byteEnd,
                std::vector<Rect>& out) {
    GYO_ASSERT(byteBegin <= byteEnd);
    GYO_ASSERT(IsBoundary(layout, byteBegin) && IsBoundary(layout, byteEnd));
    for (const LayoutLine& line : layout.lines) {
        const std::uint32_t b = std::max(byteBegin, line.byteBegin);
        const std::uint32_t e = std::min(byteEnd, line.byteEnd);
        if (b >= e) {
            continue;
        }
        const float x0 = XAt(layout, line, b);
        const float x1 = XAt(layout, line, e);
        out.push_back(Rect{x0, line.top, x1 - x0, line.height});
    }
}

} // namespace GYO::Engine::Text
