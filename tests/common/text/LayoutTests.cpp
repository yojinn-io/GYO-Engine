#include "doctest/doctest.h"

#include <string>

#include "TextTestSupport.hpp"

// Line breaking and placement (Design Invariant TX-4; declaration 5.5). At
// 16 px: Latin A, V, hyphen 8 px, space 4 px; every kana and CJK
// punctuation 16 px; ascender 820 and descender 204 units.

using namespace GYO::Engine;
using GYO::Test::Text::Fixture;
using GYO::Test::Text::Units16;

namespace {

std::vector<std::string> Lines(const Text::TextLayout& layout, const std::string& text) {
    std::vector<std::string> out;
    for (const Text::LayoutLine& line : layout.lines) {
        out.push_back(text.substr(line.byteBegin, line.byteEnd - line.byteBegin));
    }
    return out;
}

Text::LayoutOptions Width(const float width, const Text::HorizontalAlign align = Text::HorizontalAlign::Left) {
    return Text::LayoutOptions{width, align};
}

} // namespace

TEST_CASE("text layout: one line, the line box from the font's metrics") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("A", f.Style(f.latinFirst));
    REQUIRE(layout.lines.size() == 1);
    CHECK(layout.lines[0].top == 0.0f);
    CHECK(layout.lines[0].baseline == Units16(820));
    CHECK(layout.lines[0].height == 16.0f);
    CHECK(layout.firstBaseline == Units16(820));
    CHECK(layout.height == 16.0f);
    CHECK(layout.glyphs[0].y == Units16(820));
    // RelativeToFont(1.2): 1.2 x 1024 = 1228.8 -> 1229; leading 205 -> 102 above, 103 below.
    const Text::TextLayout relative = f.Lay("A", f.Style(f.latinFirst, 16.0f, Text::LineHeight::Relative(1.2f)));
    CHECK(relative.lines[0].baseline == Units16(820 + 102));
    CHECK(relative.lines[0].height == Units16(1229));
}

TEST_CASE("text layout: an empty text is one empty line of the span's font") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("", f.Style(f.latinFirst));
    REQUIRE(layout.lines.size() == 1);
    CHECK(layout.glyphs.empty());
    CHECK(layout.lines[0].height == 16.0f);
    CHECK(layout.lines[0].byteBegin == 0);
    CHECK(layout.lines[0].byteEnd == 0);
    CHECK(layout.graphemeBoundaries == std::vector<std::uint32_t>{0});
}

TEST_CASE("text layout: mandatory breaks end lines; a final one leaves an empty line") {
    Fixture f;
    const std::string text = "A\nB\r\nC\n";
    const Text::TextLayout layout = f.Lay(text, f.Style(f.latinFirst));
    CHECK(Lines(layout, text) == std::vector<std::string>{"A\n", "B\r\n", "C\n", ""});
    CHECK(layout.lines[0].hardBreak);
    CHECK(layout.lines[1].hardBreak);
    CHECK(layout.lines[2].hardBreak);
    CHECK_FALSE(layout.lines[3].hardBreak);
    CHECK(layout.lines[1].top == 16.0f);
    CHECK(layout.height == 64.0f);
    // The line break is laid out but not drawn and has no width.
    CHECK_FALSE(layout.glyphs[1].visible);
    CHECK(layout.lines[0].width == 8.0f);
}

TEST_CASE("text layout: lines break at the last opportunity that fits; trailing spaces hang") {
    Fixture f;
    const std::string text = "AA AA AA";
    // "AA AA " is 36 px; the third word would make it 56.
    const Text::TextLayout layout = f.Lay(text, f.Style(f.latinFirst), Width(40.0f));
    CHECK(Lines(layout, text) == std::vector<std::string>{"AA AA ", "AA"});
    CHECK(layout.lines[0].width == 36.0f);
    CHECK_FALSE(layout.lines[0].hardBreak);
    CHECK(layout.lines[1].width == 16.0f);
    CHECK(layout.width == 36.0f);
    // No width: only the mandatory breaks.
    CHECK(f.Lay(text, f.Style(f.latinFirst)).lines.size() == 1);
}

TEST_CASE("text layout: a word wider than the line overflows instead of being cut") {
    Fixture f;
    const std::string text = "AAAAAAAAAA AA";
    const Text::TextLayout layout = f.Lay(text, f.Style(f.latinFirst), Width(20.0f));
    CHECK(Lines(layout, text) == std::vector<std::string>{"AAAAAAAAAA ", "AA"});
    CHECK(layout.lines[0].width == 80.0f);
}

TEST_CASE("text layout: one shaped text, laid out at several widths") {
    Fixture f;
    const Text::ShapedText shaped = f.Shape("AA AA AA", f.Style(f.latinFirst));
    CHECK(f.context.Layout(shaped, Width(100.0f)).lines.size() == 1);
    CHECK(f.context.Layout(shaped, Width(40.0f)).lines.size() == 2);
    CHECK(f.context.Layout(shaped, Width(10.0f)).lines.size() == 3);
}

TEST_CASE("text layout: Japanese line breaking keeps closing marks and small kana off the line start") {
    Fixture f;
    // Ten kana fill 160 px exactly.
    // 。 may not start a line: the last kana goes down with it.
    const std::string closing = "あいうえおかきくけこ。";
    CHECK(Lines(f.Lay(closing, f.Style(f.kanaFirst), Width(160.0f)), closing) ==
          std::vector<std::string>{"あいうえおかきくけ", "こ。"});
    // っ (a small kana, CJ resolved as NS) may not start a line either.
    const std::string small = "あいうえおかきくけこっ";
    CHECK(Lines(f.Lay(small, f.Style(f.kanaFirst), Width(160.0f)), small) ==
          std::vector<std::string>{"あいうえおかきくけ", "こっ"});
    // 「 may not end a line: it goes down with what it opens.
    const std::string opening = "あいうえおかきくけ「こ」";
    CHECK(Lines(f.Lay(opening, f.Style(f.kanaFirst), Width(160.0f)), opening) ==
          std::vector<std::string>{"あいうえおかきくけ", "「こ」"});
    // Between kana any place may break.
    const std::string plain = "あいうえおかきくけこさ";
    CHECK(Lines(f.Lay(plain, f.Style(f.kanaFirst), Width(160.0f)), plain) ==
          std::vector<std::string>{"あいうえおかきくけこ", "さ"});
}

TEST_CASE("text layout: mixed Japanese and English breaks between words, not inside them") {
    Fixture f;
    const std::string text = "あいPress Start";
    // あい 32 + Press 40: 72; + space 4 + Start 40 = 116 > 100.
    const Text::TextLayout layout = f.Lay(text, f.Style(f.kanaFirst), Width(100.0f));
    CHECK(Lines(layout, text) == std::vector<std::string>{"あいPress ", "Start"});
}

TEST_CASE("text layout: a break where shaping is unsafe shapes each side again") {
    Fixture f;
    // hyphen V kerns by -64 units; a line may break after the hyphen.
    const std::string text = "A-V";
    const Text::TextLayout whole = f.Lay(text, f.Style(f.latinFirst));
    REQUIRE(whole.lines.size() == 1);
    CHECK(whole.glyphs[1].advance == Units16(512 - 64));
    const Text::TextLayout broken = f.Lay(text, f.Style(f.latinFirst), Width(20.0f));
    CHECK(Lines(broken, text) == std::vector<std::string>{"A-", "V"});
    // Without V after it, the hyphen does not kern.
    CHECK(broken.glyphs[1].advance == Units16(512));
    CHECK(broken.lines[0].width == 16.0f);
}

TEST_CASE("text layout: alignment within the width, or within the widest line") {
    Fixture f;
    const Text::TextLayout center = f.Lay("AA", f.Style(f.latinFirst), Width(100.0f, Text::HorizontalAlign::Center));
    CHECK(center.lines[0].x == 42.0f);
    CHECK(center.glyphs[0].x == 42.0f);
    CHECK(center.glyphs[0].penX == 42.0f);
    const Text::TextLayout right = f.Lay("AA", f.Style(f.latinFirst), Width(100.0f, Text::HorizontalAlign::Right));
    CHECK(right.glyphs[0].x == 84.0f);
    const std::string text = "AAAA\nA";
    const Text::TextLayout widest = f.Lay(text, f.Style(f.latinFirst), {std::nullopt, Text::HorizontalAlign::Right});
    CHECK(widest.lines[1].x == 24.0f);
    CHECK(widest.width == 32.0f);
}
