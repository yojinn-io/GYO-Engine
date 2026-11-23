#include "doctest/doctest.h"

#include <random>
#include <string>

#include "TextTestSupport.hpp"

// Shaping, fallback and the source ranges (Design Invariants TX-2, TX-3,
// TX-5). Expected values come from generate_test_fonts.py at 16 px, where a
// font unit is 1/64 px: Latin glyphs are 512 units (8 px) unless named there.

using namespace GYO::Engine;
using GYO::Test::Text::Fixture;
using GYO::Test::Text::Units16;

namespace {

struct Range {
    std::uint32_t begin;
    std::uint32_t end;
};

void CheckRanges(const Text::TextLayout& layout, const std::vector<Range>& expected) {
    REQUIRE(layout.glyphs.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CAPTURE(i);
        CHECK(layout.glyphs[i].byteBegin == expected[i].begin);
        CHECK(layout.glyphs[i].byteEnd == expected[i].end);
    }
}

// Clusters in text order, touching, covering every byte once (TX-3).
void CheckCover(const Text::TextLayout& layout) {
    std::uint32_t covered = 0;
    for (std::size_t i = 0; i < layout.glyphs.size(); ++i) {
        const Text::PositionedGlyph& glyph = layout.glyphs[i];
        CHECK(glyph.byteBegin < glyph.byteEnd);
        if (i > 0 && layout.glyphs[i - 1].byteBegin == glyph.byteBegin) {
            CHECK(layout.glyphs[i - 1].byteEnd == glyph.byteEnd);
            continue;
        }
        CHECK(glyph.byteBegin == covered);
        covered = glyph.byteEnd;
    }
    CHECK(covered == layout.textBytes);
}

} // namespace

TEST_CASE("text shape: advances are the font's, scaled exactly") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("Ai W", f.Style(f.latinFirst));
    REQUIRE(layout.glyphs.size() == 4);
    CHECK(layout.glyphs[0].advance == Units16(512));
    CHECK(layout.glyphs[1].advance == Units16(256));
    CHECK(layout.glyphs[2].advance == Units16(256));
    CHECK(layout.glyphs[3].advance == Units16(768));
    CHECK(layout.glyphs[3].penX == Units16(512 + 256 + 256));
    CHECK(layout.width == Units16(512 + 256 + 256 + 768));
    // 32 px: twice the advance.
    const Text::TextLayout big = f.Lay("W", f.Style(f.latinFirst, 32.0f));
    CHECK(big.glyphs[0].advance == 2 * Units16(768));
}

TEST_CASE("text shape: kerning moves the pair together") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("AV", f.Style(f.latinFirst));
    REQUIRE(layout.glyphs.size() == 2);
    CHECK(layout.glyphs[0].advance == Units16(512 - 128));
    CHECK(layout.glyphs[1].x == Units16(512 - 128));
    CHECK(layout.width == Units16(1024 - 128));
}

TEST_CASE("text shape: a ligature is one glyph for two characters") {
    Fixture f;
    const Text::TextLayout layout = f.Lay("fix", f.Style(f.latinFirst));
    CheckRanges(layout, {{0, 2}, {2, 3}});
    CHECK(layout.glyphs[0].advance == Units16(600));
    CHECK(layout.glyphs[1].x == Units16(600));
}

TEST_CASE("text shape: a decomposed character is two glyphs with one range") {
    Fixture f;
    // U+00E4 (2 bytes) becomes a + U+0308 through 'ccmp'.
    const Text::TextLayout layout = f.Lay("\xC3\xA4" "b", f.Style(f.latinFirst));
    CheckRanges(layout, {{0, 2}, {0, 2}, {2, 3}});
    CHECK(layout.glyphs[0].advance == Units16(512));
    CHECK(layout.glyphs[1].advance == 0.0f);
    CHECK(layout.glyphs[2].penX == Units16(512));
}

TEST_CASE("text shape: a combining mark stays in its base's cluster and grapheme") {
    Fixture f;
    // e + U+0301: one grapheme of three bytes.
    const Text::TextLayout layout = f.Lay("e\xCC\x81z", f.Style(f.latinFirst));
    CheckRanges(layout, {{0, 3}, {0, 3}, {3, 4}});
    CHECK(layout.graphemeBoundaries == std::vector<std::uint32_t>{0, 3, 4});
}

TEST_CASE("text shape: fallback takes the first font with the glyph, per grapheme") {
    Fixture f;
    // "Aあ": A from Latin, あ (3 bytes) from Kana.
    const Text::TextLayout layout = f.Lay("A\xE3\x81\x82" "B", f.Style(f.latinFirst));
    CheckRanges(layout, {{0, 1}, {1, 4}, {4, 5}});
    CHECK(layout.glyphs[0].font == f.latin);
    CHECK(layout.glyphs[1].font == f.kana);
    CHECK(layout.glyphs[2].font == f.latin);
    CHECK(layout.glyphs[1].x == Units16(512));
    CHECK(layout.glyphs[2].x == Units16(512 + 1024));
    // The Kana font first: A is not in it, so it falls back to Latin.
    const Text::TextLayout kanaFirst = f.Lay("A\xE3\x81\x82", f.Style(f.kanaFirst));
    CHECK(kanaFirst.glyphs[0].font == f.latin);
    CHECK(kanaFirst.glyphs[1].font == f.kana);
}

TEST_CASE("text shape: a character no font has is the primary font's .notdef") {
    Fixture f;
    // U+20AC is in neither font.
    const Text::TextLayout layout = f.Lay("\xE2\x82\xAC", f.Style(f.kanaFirst));
    CheckRanges(layout, {{0, 3}});
    CHECK(layout.glyphs[0].font == f.kana);
    CHECK(layout.glyphs[0].glyph == 0);
    CHECK(layout.glyphs[0].advance == Units16(1024));
    CHECK(layout.glyphs[0].visible);
}

TEST_CASE("text shape: a default-ignorable character does not decide the font") {
    Fixture f;
    // あ + U+FE00 (variation selector, in neither font): still the Kana font.
    const Text::TextLayout layout = f.Lay("\xE3\x81\x82\xEF\xB8\x80", f.Style(f.latinFirst));
    REQUIRE_FALSE(layout.glyphs.empty());
    CHECK(layout.glyphs[0].font == f.kana);
    CHECK(layout.graphemeBoundaries == std::vector<std::uint32_t>{0, 6});
    CheckCover(layout);
}

TEST_CASE("text shape: ill-formed UTF-8 is U+FFFD per maximal subpart, keeping its bytes") {
    Fixture f;
    // A, a lone 0xFF, B: U+FFFD is 700 units in the Latin font.
    const Text::TextLayout lone = f.Lay("A\xFF" "B", f.Style(f.latinFirst));
    CheckRanges(lone, {{0, 1}, {1, 2}, {2, 3}});
    CHECK(lone.glyphs[1].advance == Units16(700));
    // A truncated three-byte sequence (the first two bytes of あ) is one subpart.
    const Text::TextLayout truncated = f.Lay("\xE3\x81" "A", f.Style(f.latinFirst));
    CheckRanges(truncated, {{0, 2}, {2, 3}});
    CHECK(truncated.glyphs[0].advance == Units16(700));
    // A surrogate encoded in UTF-8 (ED A0 80): ED, then A0 and 80 each alone.
    const Text::TextLayout surrogate = f.Lay("\xED\xA0\x80", f.Style(f.latinFirst));
    CheckRanges(surrogate, {{0, 1}, {1, 2}, {2, 3}});
}

TEST_CASE("text shape: spans of different sizes on one line") {
    Fixture f;
    const std::string text = "AA";
    const Text::TextSpan spans[] = {{0, 1, f.Style(f.latinFirst, 16.0f)}, {1, 2, f.Style(f.latinFirst, 32.0f)}};
    const Text::TextLayout layout = f.context.Layout(f.context.Shape(text, spans), {});
    REQUIRE(layout.glyphs.size() == 2);
    CHECK(layout.glyphs[0].span == 0);
    CHECK(layout.glyphs[1].span == 1);
    CHECK(layout.glyphs[1].advance == 2 * Units16(512));
    CHECK(layout.glyphs[1].sizePx == 32.0f);
    // Line box: each span's own line height (16 px and 16 px) around its own
    // ascender and descender; the 32 px span's 1640/408 units exceed 16 px.
    REQUIRE(layout.lines.size() == 1);
    // 32 px: ascender 1640, descender 408 (26.6); line height 1024 (16 px):
    // leading -1024, half -512 → ascent 1128, descent 408 - 512 = -104;
    // 16 px span: ascent 820, descent 204. Max: 1128 and 204.
    CHECK(layout.lines[0].baseline == Units16(1128));
    CHECK(layout.lines[0].height == Units16(1128 + 204));
}

TEST_CASE("text shape: clusters cover the text in order for any input") {
    Fixture f;
    const std::vector<std::string> pieces{"A", "i", "f", " ", "V", "-", "\n", "\t", "\xE3\x81\x82", "\xE3\x81\xA3",
                                          "\xE3\x80\x82", "\xCC\x81", "\xC3\xA4", "\xFF", "\xE3\x81", "\xEF\xB8\x80",
                                          "\xE2\x80\x8D", "\xF0\x9F\x98\x80", "\r\n"};
    std::mt19937 random(20261123u);
    for (int round = 0; round < 300; ++round) {
        std::string text;
        const int length = static_cast<int>(random() % 12);
        for (int i = 0; i < length; ++i) {
            text += pieces[random() % pieces.size()];
        }
        CAPTURE(text);
        const Text::TextLayout layout = f.Lay(text, f.Style(f.latinFirst), Text::LayoutOptions{40.0f, {}});
        CheckCover(layout);
        CHECK(layout.graphemeBoundaries.front() == 0);
        CHECK(layout.graphemeBoundaries.back() == text.size());
        // Lines touch and cover the text.
        CHECK(layout.lines.front().byteBegin == 0);
        CHECK(layout.lines.back().byteEnd == text.size());
        for (std::size_t l = 1; l < layout.lines.size(); ++l) {
            CHECK(layout.lines[l - 1].byteEnd == layout.lines[l].byteBegin);
        }
    }
}
