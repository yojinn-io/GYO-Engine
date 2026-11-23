#include "doctest/doctest.h"

#include <algorithm>
#include <string>

#include "TextTestSupport.hpp"
#include "engine/text/Caret.hpp"

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
    // At 38 px the space after "AA AA" reaches 40: it hangs and does not break the line.
    CHECK(Lines(f.Lay(text, f.Style(f.latinFirst), Width(38.0f)), text) == std::vector<std::string>{"AA AA ", "AA"});
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
    // After a break the new line already holds the kana that did not fit: every line is two kana wide.
    const std::string kana = "あいうえお";
    const Text::TextLayout two = f.Lay(kana, f.Style(f.kanaFirst), Width(32.0f));
    CHECK(Lines(two, kana) == std::vector<std::string>{"あい", "うえ", "お"});
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

TEST_CASE("text layout: the language and the strictness are the caller's choice") {
    Fixture f;
    const auto lay = [&](const std::string& text, const Text::ShapeOptions& options) {
        const Text::TextSpan span{0, static_cast<std::uint32_t>(text.size()), f.Style(f.kanaFirst)};
        return Lines(f.context.Layout(f.context.Shape(text, {&span, 1}, options), Width(160.0f)), text);
    };
    // Normal: a small kana may start a line.
    const std::string small = "あいうえおかきくけこっ";
    CHECK(lay(small, {}) == std::vector<std::string>{"あいうえおかきくけ", "こっ"});
    CHECK(lay(small, Text::ShapeOptions{{}, Text::LineBreakStrictness::Normal}) ==
          std::vector<std::string>{"あいうえおかきくけこ", "っ"});
    // ① (U+2460, ambiguous): a letter without a language, an ideograph in Japanese.
    // Eleven of them (the Kana font's .notdef, 16 px each).
    std::string circled;
    for (int i = 0; i < 11; ++i) {
        circled += "\xE2\x91\xA0";
    }
    CHECK(lay(circled, {}).size() == 1);
    CHECK(lay(circled, Text::ShapeOptions{"ja", Text::LineBreakStrictness::Strict}).size() == 2);
}

TEST_CASE("text layout: mixed Japanese and English breaks between words, not inside them") {
    Fixture f;
    const std::string text = "あいPress Start";
    // あい 32 + Press 40: 72; + space 4 + Start 40 = 116 > 100.
    const Text::TextLayout layout = f.Lay(text, f.Style(f.kanaFirst), Width(100.0f));
    CHECK(Lines(layout, text) == std::vector<std::string>{"あいPress ", "Start"});
}

TEST_CASE("text layout: other mandatory breaks, and the ideographic space hangs") {
    Fixture f;
    // U+2028 LINE SEPARATOR, U+0085 NEXT LINE, VT and FF all end a line.
    const std::string text = "A\xE2\x80\xA8" "B\xC2\x85" "C\x0B" "D\x0C" "E";
    const Text::TextLayout layout = f.Lay(text, f.Style(f.latinFirst));
    CHECK(Lines(layout, text) == std::vector<std::string>{"A\xE2\x80\xA8", "B\xC2\x85", "C\x0B", "D\x0C", "E"});
    CHECK_FALSE(layout.glyphs[1].visible);
    // U+3000 at the end of a full line hangs: it does not push the line over.
    const std::string kana = "あいうえおかきくけこ　さ";
    CHECK(Lines(f.Lay(kana, f.Style(f.kanaFirst), Width(160.0f)), kana) ==
          std::vector<std::string>{"あいうえおかきくけこ　", "さ"});
}

TEST_CASE("text layout: the empty line after a final break takes the last span's line height") {
    Fixture f;
    const std::string text = "A\n";
    const Text::TextSpan spans[] = {{0, 1, f.Style(f.latinFirst, 16.0f, Text::LineHeight::Pixels(16.0f))},
                                    {1, 2, f.Style(f.latinFirst, 16.0f, Text::LineHeight::Pixels(32.0f))}};
    const Text::TextLayout layout = f.context.Layout(f.context.Shape(text, spans), {});
    REQUIRE(layout.lines.size() == 2);
    CHECK(layout.lines[1].height == 32.0f);
}

TEST_CASE("text layout: a negative odd leading puts the extra unit above") {
    Fixture f;
    // Line height 1023 units: leading -1, floor(-1 / 2) = -1 above, 0 below.
    const Text::TextLayout layout =
        f.Lay("A", f.Style(f.latinFirst, 16.0f, Text::LineHeight::Pixels(1023.0f / 64.0f)));
    CHECK(layout.lines[0].baseline == Units16(819));
    CHECK(layout.lines[0].height == Units16(1023));
}

TEST_CASE("text layout: the fallback font's metrics count in the line box") {
    Fixture f;
    const Text::FontId biz = f.context.AddFont(GYO::Test::Text::LoadFont("BIZUDPGothic-Subset.ttf"), 0).value();
    const std::vector<Text::FontId> mixed{f.latin, biz};
    const std::vector<Text::FontId> bizOnly{biz};
    const auto relative = Text::LineHeight::Relative(1.0f);
    const float latinBaseline = f.Lay("A", f.Style(f.latinFirst, 16.0f, relative)).lines[0].baseline;
    const float bizBaseline = f.Lay("\xE3\x81\x86", f.Style(bizOnly, 16.0f, relative)).lines[0].baseline;
    REQUIRE(latinBaseline != bizBaseline);
    // う (in the BIZ subset, not in the Latin font).
    const Text::TextLayout layout = f.Lay("A\xE3\x81\x86", f.Style(mixed, 16.0f, relative));
    CHECK(layout.glyphs[1].font == biz);
    CHECK(layout.lines[0].baseline == std::max(latinBaseline, bizBaseline));
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
    // Together, V is placed 64 units right and 128 up; alone on its line it is not.
    CHECK(whole.glyphs[2].x == whole.glyphs[2].penX + Units16(64));
    CHECK(whole.glyphs[2].y == whole.lines[0].baseline - Units16(128));
    CHECK(broken.glyphs[2].x == broken.glyphs[2].penX);
    CHECK(broken.glyphs[2].y == broken.lines[1].baseline);
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
    // An empty last line is aligned too: the caret sits in the middle.
    const Text::TextLayout empty = f.Lay("A\n", f.Style(f.latinFirst), Width(100.0f, Text::HorizontalAlign::Center));
    CHECK(Text::CaretAt(empty, Text::Caret{2}).x == 50.0f);
}
