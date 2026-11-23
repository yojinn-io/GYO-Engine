#include "doctest/doctest.h"

#include <string>

#include "TextTestSupport.hpp"
#include "engine/text/Caret.hpp"
#include "AssertTestSupport.hpp"

// Caret, hit test and range rectangles (Design Invariant TX-3; declaration
// 5.6): built only from the layout's clusters, grapheme boundaries and lines.

using namespace GYO::Engine;
using GYO::Test::Text::Fixture;
using GYO::Test::Text::Units16;
using Text::Caret;
using Text::CaretAffinity;

TEST_CASE("text caret: at cluster starts, on the pen") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("AiW", f.Style(f.latinFirst));
    CHECK(Text::CaretAt(layout, Caret{0}).x == 0.0f);
    CHECK(Text::CaretAt(layout, Caret{1}).x == 8.0f);
    CHECK(Text::CaretAt(layout, Caret{2}).x == 12.0f);
    CHECK(Text::CaretAt(layout, Caret{3}).x == 24.0f);
    const Text::CaretRect rect = Text::CaretAt(layout, Caret{1});
    CHECK(rect.top == 0.0f);
    CHECK(rect.height == 16.0f);
    CHECK(rect.line == 0);
}

TEST_CASE("text caret: inside a ligature, a share of its advance per grapheme") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("fi", f.Style(f.latinFirst));
    REQUIRE(layout.glyphs.size() == 1);
    CHECK(Text::CaretAt(layout, Caret{1}).x == Units16(600) / 2.0f);
    CHECK(Text::CaretAt(layout, Caret{2}).x == Units16(600));
}

TEST_CASE("text caret: only on grapheme boundaries") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("e\xCC\x81", f.Style(f.latinFirst));
    CHECK(Text::CaretAt(layout, Caret{3}).x == Units16(512));
    GYO_CHECK_ASSERTS(Text::CaretAt(layout, Caret{1}));
    GYO_CHECK_ASSERTS(Text::CaretAt(layout, Caret{9}));
}

TEST_CASE("text caret: at a soft break the affinity picks the line") {
    Fixture f;
    // "AA " | "AA": offset 3 ends line 0 and starts line 1.
    const Text::TextLayout layout = f.Lay("AA AA", f.Style(f.latinFirst), Text::LayoutOptions{20.0f, {}});
    REQUIRE(layout.lines.size() == 2);
    const Text::CaretRect up = Text::CaretAt(layout, Caret{3, CaretAffinity::Upstream});
    CHECK(up.line == 0);
    CHECK(up.x == 20.0f);
    const Text::CaretRect down = Text::CaretAt(layout, Caret{3, CaretAffinity::Downstream});
    CHECK(down.line == 1);
    CHECK(down.x == 0.0f);
    CHECK(down.top == 16.0f);
}

TEST_CASE("text caret: after a mandatory break the caret is on the next line") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("A\nB", f.Style(f.latinFirst));
    CHECK(Text::CaretAt(layout, Caret{1}).line == 0);
    CHECK(Text::CaretAt(layout, Caret{2, CaretAffinity::Upstream}).line == 1);
    CHECK(Text::CaretAt(layout, Caret{2}).x == 0.0f);
    // An empty last line after a final line feed.
    const Text::TextLayout trailing = f.Lay("A\n", f.Style(f.latinFirst));
    CHECK(Text::CaretAt(trailing, Caret{2}).line == 1);
    // An empty text.
    const Text::TextLayout empty = f.Lay("", f.Style(f.latinFirst));
    CHECK(Text::CaretAt(empty, Caret{0}).height == 16.0f);
}

TEST_CASE("text hit test: the nearest caret on the line the point is on") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("AA AA", f.Style(f.latinFirst), Text::LayoutOptions{20.0f, {}});
    CHECK(Text::HitTest(layout, 3.0f, 4.0f) == Caret{0});
    CHECK(Text::HitTest(layout, 7.0f, 4.0f) == Caret{1});
    // Right of line 0: its end, upstream.
    CHECK(Text::HitTest(layout, 99.0f, 4.0f) == Caret{3, CaretAffinity::Upstream});
    // Line 1; above and below the text clamp to the first and last line.
    CHECK(Text::HitTest(layout, 9.0f, 20.0f) == Caret{4});
    CHECK(Text::HitTest(layout, 0.0f, -50.0f) == Caret{0});
    CHECK(Text::HitTest(layout, 99.0f, 500.0f) == Caret{5});
    // Before a mandatory break, not after it.
    const Text::TextLayout hard = f.Lay("A\nB", f.Style(f.latinFirst));
    CHECK(Text::HitTest(hard, 99.0f, 4.0f) == Caret{1});
    // CaretAt and HitTest agree on every boundary.
    for (const std::uint32_t offset : layout.graphemeBoundaries) {
        for (const CaretAffinity affinity : {CaretAffinity::Downstream, CaretAffinity::Upstream}) {
            const Text::CaretRect rect = Text::CaretAt(layout, Caret{offset, affinity});
            const Caret hit = Text::HitTest(layout, rect.x, rect.top + 1.0f);
            CAPTURE(offset);
            CHECK(Text::CaretAt(layout, hit).x == rect.x);
            CHECK(Text::CaretAt(layout, hit).line == rect.line);
        }
    }
}

TEST_CASE("text range: one rectangle per line, for a selection or an IME composition") {
    Fixture f;
    // "Aがっこう": the composition is bytes [1, 13).
    const std::string text = "A\xE3\x81\x8C\xE3\x81\xA3\xE3\x81\x93\xE3\x81\x86";
    const Text::TextLayout layout = f.Lay(text, f.Style(f.latinFirst));
    std::vector<Text::Rect> rects;
    Text::RangeRects(layout, 1, 13, rects);
    REQUIRE(rects.size() == 1);
    CHECK(rects[0] == Text::Rect{8.0f, 0.0f, 64.0f, 16.0f});
    // Over a soft break: two rectangles.
    const Text::TextLayout wrapped = f.Lay("AA AA", f.Style(f.latinFirst), Text::LayoutOptions{20.0f, {}});
    rects.clear();
    Text::RangeRects(wrapped, 1, 4, rects);
    REQUIRE(rects.size() == 2);
    CHECK(rects[0] == Text::Rect{8.0f, 0.0f, 12.0f, 16.0f});
    CHECK(rects[1] == Text::Rect{0.0f, 16.0f, 8.0f, 16.0f});
    // An empty range has none.
    rects.clear();
    Text::RangeRects(wrapped, 2, 2, rects);
    CHECK(rects.empty());
}
