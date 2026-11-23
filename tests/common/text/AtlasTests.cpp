#include "doctest/doctest.h"

#include <algorithm>
#include <string>

#include "TextTestSupport.hpp"
#include "AssertTestSupport.hpp"

// Rasterizing into the glyph atlas (Design Invariant TX-6; declaration 5.7).

using namespace GYO::Engine;
using GYO::Test::Text::Fixture;
using GYO::Test::Text::LoadFont;

namespace {

// What a renderer that only follows ChangesSince holds: its copy of every page.
struct Reader {
    std::uint64_t revision = 0;
    std::vector<std::vector<std::uint8_t>> pages;
    std::size_t wholePages = 0;
    std::size_t rects = 0;

    void Follow(const Text::GlyphAtlas& atlas) {
        const Text::AtlasChanges changes = atlas.ChangesSince(revision);
        for (const Text::AtlasChange& change : changes.changes) {
            const Text::AtlasPageView page = atlas.Page(change.page);
            pages.resize(std::max<std::size_t>(pages.size(), change.page + 1));
            pages[change.page].resize(page.pixels.size());
            if (change.rect.width == page.width && change.rect.height == page.height) {
                ++wholePages;
            } else {
                ++rects;
            }
            for (std::uint32_t row = 0; row < change.rect.height; ++row) {
                const std::size_t at = static_cast<std::size_t>(change.rect.y + row) * page.rowPitch + change.rect.x;
                std::copy_n(page.pixels.begin() + static_cast<std::ptrdiff_t>(at), change.rect.width,
                            pages[change.page].begin() + static_cast<std::ptrdiff_t>(at));
            }
        }
        revision = changes.revision;
    }

    bool Matches(const Text::GlyphAtlas& atlas) const {
        if (pages.size() != atlas.PageCount()) {
            return false;
        }
        for (std::size_t p = 0; p < pages.size(); ++p) {
            const Text::AtlasPageView page = atlas.Page(p);
            if (!std::equal(page.pixels.begin(), page.pixels.end(), pages[p].begin(), pages[p].end())) {
                return false;
            }
        }
        return true;
    }
};

} // namespace

TEST_CASE("text atlas: a glyph is drawn once; the same text again changes nothing") {
    Fixture f;
    Text::GlyphAtlas atlas;
    const Text::TextLayout layout = f.Lay("AAV", f.Style(f.latinFirst));
    const Text::RasterResult first = f.context.Rasterize(layout, atlas);
    CHECK(first.newGlyphs == 2);
    REQUIRE(first.placements.size() == 3);
    CHECK(first.placements[0].status == Text::GlyphRasterStatus::Placed);
    CHECK(first.placements[0].rect == first.placements[1].rect);
    CHECK(atlas.GlyphCount() == 2);
    CHECK(atlas.Revision() == 1);
    const Text::RasterResult second = f.context.Rasterize(layout, atlas);
    CHECK(second.newGlyphs == 0);
    CHECK(atlas.Revision() == 1);
    CHECK(atlas.ChangesSince(1).changes.empty());
    // Another size is another glyph.
    const Text::RasterResult big = f.context.Rasterize(f.Lay("A", f.Style(f.latinFirst, 32.0f)), atlas);
    CHECK(big.newGlyphs == 1);
    CHECK(atlas.Revision() == 2);
}

TEST_CASE("text atlas: Alpha8 pages, row 0 on top, padding left empty") {
    Fixture f;
    Text::GlyphAtlas atlas(Text::AtlasOptions{64, 1, 4096});
    const Text::RasterResult result = f.context.Rasterize(f.Lay("A", f.Style(f.latinFirst)), atlas);
    REQUIRE(result.placements[0].status == Text::GlyphRasterStatus::Placed);
    const Text::AtlasPageView page = atlas.Page(0);
    CHECK(page.format == Text::AtlasFormat::Alpha8);
    CHECK(page.width == 64);
    CHECK(page.rowPitch == 64);
    const Text::AtlasRect rect = result.placements[0].rect;
    CHECK(rect.x == 1);
    CHECK(rect.y == 1);
    // The test glyph is a box from 32 to 480 units wide (0.5..7.5 px) and 0 to
    // 700 units tall (0..10.9 px): an 8 x 11 bitmap whose top is 11 px above
    // the baseline.
    CHECK(rect.width == 8);
    CHECK(rect.height == 11);
    CHECK(result.placements[0].left == 0);
    CHECK(result.placements[0].top == 11);
    // Fully covered inside, nothing in the padding row above.
    CHECK(page.pixels[static_cast<std::size_t>(rect.y + 5) * 64 + rect.x + 4] == 255);
    for (std::uint32_t x = 0; x < rect.width + 2; ++x) {
        CHECK(page.pixels[x] == 0);
    }
}

TEST_CASE("text atlas: line breaks and invisible glyphs are not drawn") {
    Fixture f;
    Text::GlyphAtlas atlas;
    const Text::RasterResult result = f.context.Rasterize(f.Lay("A\n \tB", f.Style(f.latinFirst)), atlas);
    REQUIRE(result.placements.size() == 5);
    CHECK(result.placements[1].status == Text::GlyphRasterStatus::Empty);
    // A space is drawn as nothing (an empty bitmap).
    CHECK(result.placements[2].status == Text::GlyphRasterStatus::Empty);
    CHECK(result.placements[3].status == Text::GlyphRasterStatus::Empty);
    CHECK(result.newGlyphs == 2);
    // The Kana font alone has no space: a line break would be its .notdef, and is still not drawn.
    const std::vector<Text::FontId> kanaOnly{f.kana};
    const Text::RasterResult kana = f.context.Rasterize(f.Lay("\n\t", f.Style(kanaOnly)), atlas);
    CHECK(kana.newGlyphs == 0);
}

TEST_CASE("text atlas: a glyph larger than a page is TooLarge, not an error") {
    Fixture f;
    Text::GlyphAtlas atlas(Text::AtlasOptions{16, 1, 4096});
    const Text::RasterResult result = f.context.Rasterize(f.Lay("AB", f.Style(f.latinFirst, 64.0f)), atlas);
    CHECK(result.placements[0].status == Text::GlyphRasterStatus::TooLarge);
    CHECK(result.tooLarge == 2);
    CHECK(result.newGlyphs == 0);
    CHECK(atlas.PageCount() == 0);
}

TEST_CASE("text atlas: full pages open new ones; a reader following the changes holds the same pixels") {
    Fixture f;
    Text::GlyphAtlas atlas(Text::AtlasOptions{32, 1, 4096});
    Reader reader;
    const std::string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (const char letter : letters) {
        const Text::RasterResult result = f.context.Rasterize(f.Lay(std::string(1, letter), f.Style(f.latinFirst)), atlas);
        CHECK(result.newGlyphs == 1);
        reader.Follow(atlas);
        CHECK(reader.Matches(atlas));
    }
    CHECK(atlas.PageCount() > 1);
    CHECK(atlas.GlyphCount() == letters.size());
    CHECK(reader.wholePages == atlas.PageCount());
    CHECK(reader.rects == letters.size());
}

TEST_CASE("text atlas: a reader the change log no longer reaches gets whole pages") {
    Fixture f;
    Text::GlyphAtlas atlas(Text::AtlasOptions{64, 1, 3});
    Reader early;
    static_cast<void>(f.context.Rasterize(f.Lay("A", f.Style(f.latinFirst)), atlas));
    early.Follow(atlas);
    Reader late;
    const std::string letters = "BCDEFGHIJKLMNOP";
    for (const char letter : letters) {
        static_cast<void>(f.context.Rasterize(f.Lay(std::string(1, letter), f.Style(f.latinFirst)), atlas));
    }
    const Text::AtlasChanges changes = atlas.ChangesSince(early.revision);
    CHECK(changes.revision == atlas.Revision());
    for (const Text::AtlasChange& change : changes.changes) {
        CHECK(change.rect == Text::AtlasRect{0, 0, 64, 64});
    }
    early.Follow(atlas);
    late.Follow(atlas);
    CHECK(early.Matches(atlas));
    CHECK(late.Matches(atlas));
}

TEST_CASE("text atlas: a reader one revision behind the log's base gets whole pages") {
    Fixture f;
    // A log of one change: the first Rasterize (a page and a glyph) is dropped at once.
    Text::GlyphAtlas atlas(Text::AtlasOptions{64, 1, 1});
    Reader reader;
    static_cast<void>(f.context.Rasterize(f.Lay("A", f.Style(f.latinFirst)), atlas));
    static_cast<void>(f.context.Rasterize(f.Lay("B", f.Style(f.latinFirst)), atlas));
    reader.Follow(atlas);
    CHECK(reader.Matches(atlas));
}

TEST_CASE("text atlas: the padding counts when a glyph is too large; a cached TooLarge counts again") {
    Fixture f;
    // An 8 x 11 bitmap with 1 pixel of padding needs 13 pixels: a 12-pixel page is too small.
    Text::GlyphAtlas atlas(Text::AtlasOptions{12, 1, 4096});
    const Text::TextLayout layout = f.Lay("A", f.Style(f.latinFirst));
    const Text::RasterResult first = f.context.Rasterize(layout, atlas);
    CHECK(first.placements[0].status == Text::GlyphRasterStatus::TooLarge);
    const Text::RasterResult again = f.context.Rasterize(layout, atlas);
    CHECK(again.tooLarge == 1);
    CHECK(atlas.PageCount() == 0);
}

TEST_CASE("text atlas: an atlas serves one context; a shaped text is laid out by its own context") {
    Fixture f;
    Fixture other;
    Text::GlyphAtlas atlas;
    const Text::TextLayout layout = f.Lay("A", f.Style(f.latinFirst));
    const Text::RasterResult result = f.context.Rasterize(layout, atlas);
    CHECK(result.newGlyphs == 1);
    GYO_CHECK_ASSERTS(other.context.Rasterize(layout, atlas));
    const Text::ShapedText shaped = f.Shape("A", f.Style(f.latinFirst));
    GYO_CHECK_ASSERTS(other.context.Layout(shaped, {}));
    // A moved context is still the same context.
    Text::TextContext moved = std::move(f.context);
    CHECK(moved.Rasterize(layout, atlas).newGlyphs == 0);
    CHECK(moved.Layout(shaped, {}).glyphs.size() == 1);
}

TEST_CASE("text shape: Programmer Errors in the spans") {
    Fixture f;
    const std::vector<Text::FontId> none;
    const std::vector<Text::FontId> foreign{Text::FontId{7}};
    const std::string text = "AB";
    const Text::TextSpan gap[] = {{0, 1, f.Style(f.latinFirst)}};
    GYO_CHECK_ASSERTS(f.context.Shape(text, gap));
    const Text::TextSpan empty[] = {{0, 2, f.Style(none)}};
    GYO_CHECK_ASSERTS(f.context.Shape(text, empty));
    const Text::TextSpan unknown[] = {{0, 2, f.Style(foreign)}};
    GYO_CHECK_ASSERTS(f.context.Shape(text, unknown));
    const Text::TextSpan zero[] = {{0, 2, f.Style(f.latinFirst, 0.0f)}};
    GYO_CHECK_ASSERTS(f.context.Shape(text, zero));
    // A span boundary inside a character.
    const std::string kana = "\xE3\x81\x82";
    const Text::TextSpan split[] = {{0, 1, f.Style(f.latinFirst)}, {1, 3, f.Style(f.latinFirst)}};
    GYO_CHECK_ASSERTS(f.context.Shape(kana, split));
}
