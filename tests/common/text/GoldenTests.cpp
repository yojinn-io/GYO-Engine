#include "doctest/doctest.h"

#include <cstring>
#include <string>
#include <vector>

#include "TextTestSupport.hpp"
#include "engine/base/Sha256.hpp"

// Design Invariant TX-8 and criterion 4: the same fonts, text and options give
// the same layout and the same atlas bytes on every L1 platform. The digests
// were computed once on macOS x86_64 (2026-11-23); L1's three platforms check
// them. A different digest on any platform stops the batch (no per-platform
// branch).

using namespace GYO::Engine;
using GYO::Test::Text::LoadFont;

namespace {

void Put(std::vector<std::byte>& out, const void* data, const std::size_t size) {
    const auto* bytes = static_cast<const std::byte*>(data);
    out.insert(out.end(), bytes, bytes + size);
}

template <class T>
void Put(std::vector<std::byte>& out, const T value) {
    Put(out, &value, sizeof(value));
}

// Every field of the layout, in a fixed order (floats by their bits).
std::string LayoutDigest(const Text::TextLayout& layout) {
    std::vector<std::byte> out;
    for (const Text::PositionedGlyph& g : layout.glyphs) {
        Put(out, g.font.value);
        Put(out, g.glyph);
        Put(out, g.x);
        Put(out, g.y);
        Put(out, g.penX);
        Put(out, g.advance);
        Put(out, g.sizePx);
        Put(out, g.byteBegin);
        Put(out, g.byteEnd);
        Put(out, g.span);
        Put(out, g.line);
        Put(out, static_cast<std::uint8_t>(g.visible));
    }
    for (const Text::LayoutLine& l : layout.lines) {
        Put(out, l.top);
        Put(out, l.baseline);
        Put(out, l.height);
        Put(out, l.width);
        Put(out, l.x);
        Put(out, l.glyphBegin);
        Put(out, l.glyphEnd);
        Put(out, l.byteBegin);
        Put(out, l.byteEnd);
        Put(out, static_cast<std::uint8_t>(l.hardBreak));
    }
    for (const std::uint32_t b : layout.graphemeBoundaries) {
        Put(out, b);
    }
    Put(out, layout.width);
    Put(out, layout.height);
    Put(out, layout.firstBaseline);
    return Base::Sha256(out);
}

std::string AtlasDigest(const Text::GlyphAtlas& atlas) {
    std::vector<std::byte> out;
    for (std::size_t p = 0; p < atlas.PageCount(); ++p) {
        const Text::AtlasPageView page = atlas.Page(p);
        Put(out, page.pixels.data(), page.pixels.size());
    }
    return Base::Sha256(out);
}

struct Golden {
    std::string layout;
    std::string atlas;
};

// BIZ UDPGothic (a real TrueType Japanese font) at the sizes a 2D game's HUD
// uses, and the synthetic fonts (TrueType and CFF outlines).
Golden Run(const char* font, const std::string& text, const float size, const float width) {
    Text::TextContext context;
    const Text::FontId id = context.AddFont(LoadFont(font), 0).value();
    const std::vector<Text::FontId> fonts{id};
    const Text::TextSpan span{0, static_cast<std::uint32_t>(text.size()),
                              Text::TextStyle{fonts, size, Text::LineHeight::Relative(1.2f)}};
    const Text::TextLayout layout =
        context.Layout(context.Shape(text, {&span, 1}), Text::LayoutOptions{width, Text::HorizontalAlign::Center});
    Text::GlyphAtlas atlas(Text::AtlasOptions{256, 1, 4096});
    static_cast<void>(context.Rasterize(layout, atlas));
    return Golden{LayoutDigest(layout), AtlasDigest(atlas)};
}

const std::string kJapanese = "操作方法：ゲームパッドの「A」ボタンで決定します。残り 3 / 12";

} // namespace

TEST_CASE("text golden: BIZ UDPGothic at 12, 17, 24 and 45 px") {
    const struct {
        float size;
        const char* layout;
        const char* atlas;
    } expected[] = {
        {12.0f, "98ffcb329020b4bf44b3677d8a1eb7f9ff4becdaa31fe3b4980dd9599515012b",
         "5fe9ebc3c3ccf8406af71d2daac565c9cdaa62518f2dc55fca824fb2b12c7d78"},
        {17.0f, "a6e98ca5745af933203a13b4ee09c12c454f0e23b382a7017c8dbfd31f9f13df",
         "d95f4991a2ab6e33de03db710e2ecc9c597e3d8b40edcbc21dbb8b97fc2f7e3f"},
        {24.0f, "684d35c99021c84b227fa1bb5ad99203f9fb298bb28e1a73ce3829dc2d702f5d",
         "b4d6454cafe38b278dce6740e6eb2f4a4a1da1e55d72bc78f42b07b730b64352"},
        {45.0f, "d3f7c3da740cd17dab9c9209b4a007f60e1a46cab8a44b72ee3895d674020998",
         "24a867eefc64ae2f201b173eadcd1eefcb3978031e59dd6bb4d02b181cf527c9"},
    };
    for (const auto& e : expected) {
        CAPTURE(e.size);
        const Golden golden = Run("BIZUDPGothic-Subset.ttf", kJapanese, e.size, 300.0f);
        MESSAGE("BIZ " << e.size << " layout " << golden.layout << " atlas " << golden.atlas);
        CHECK(golden.layout == std::string(e.layout));
        CHECK(golden.atlas == std::string(e.atlas));
    }
}

TEST_CASE("text golden: the synthetic fonts, TrueType and CFF outlines") {
    const Golden latin = Run("GyoTextTestLatin.ttf", "Affix AV-V A\xC3\xA4" "e\xCC\x81 Press Start", 17.0f, 80.0f);
    MESSAGE("Latin layout " << latin.layout << " atlas " << latin.atlas);
    CHECK(latin.layout == "bd284d6e051372594011ac4edc659eccd47d8b9d70e398534a8c1a554427a2b6");
    CHECK(latin.atlas == "304742344d9d27b781b480792f5c13b365cc8056a29e259a93fa81e38a08d4a5");
    const Golden kana = Run("GyoTextTestKana.otf", "あいうえおかきくけこ。「がっこう」", 23.0f, 160.0f);
    MESSAGE("Kana layout " << kana.layout << " atlas " << kana.atlas);
    CHECK(kana.layout == "8b20b67b42c56c7c3c7b4f10963a68ef5d2238b5f4cad68e43d32ec504cacc70");
    CHECK(kana.atlas == "ac449b799bafb1925604591e8fcbc3a31f1148558d174e4fbef5b22d4f20f63b");
}
