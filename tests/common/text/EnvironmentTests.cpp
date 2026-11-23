#include "doctest/doctest.h"

#include <cstdlib>
#include <string>

#include "TextTestSupport.hpp"
#include "engine/base/Sha256.hpp"

// Design Invariants TX-7 and TX-8: the environment does not change shaping or
// rasterizing. CTest runs this executable (gyo_text_environment) through
// run_with_environment.py, which sets HB_SHAPER_LIST=fallback (HarfBuzz's
// shaper without OpenType features), HB_OPTIONS and FREETYPE_PROPERTIES (other
// hinting choices); HarfBuzz is
// built with HB_NO_GETENV and FreeType without
// FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES, so the kerning and the ligature are
// still there and the bitmaps are the ones without the variables. The
// variables come from that script, not from this file: a platform branch to set them
// belongs to engine/core/os only (os.md R2).

using namespace GYO::Engine;
using GYO::Test::Text::Fixture;
using GYO::Test::Text::Units16;

TEST_CASE("text environment: HarfBuzz and FreeType ignore the environment") {
    const char* shapers = std::getenv("HB_SHAPER_LIST");
    REQUIRE_MESSAGE(shapers != nullptr, "run through CTest's gyo_text_environment, which sets the variables");
    REQUIRE(std::string(shapers) == "fallback");
    Fixture f;
    const Text::TextLayout layout = f.Lay("AVfi", f.Style(f.latinFirst));
    REQUIRE(layout.glyphs.size() == 3);
    CHECK(layout.glyphs[0].advance == Units16(512 - 128));
    CHECK(layout.glyphs[2].advance == Units16(600));
    Text::TextContext context;
    const Text::FontId biz = context.AddFont(GYO::Test::Text::LoadFont("BIZUDPGothic-Subset.ttf"), 0).value();
    // The Kana font has CFF outlines: FreeType's CFF driver is in the picture too.
    const Text::FontId kana = context.AddFont(GYO::Test::Text::LoadFont("GyoTextTestKana.otf"), 0).value();
    const std::vector<Text::FontId> fonts{biz, kana};
    // あ and い are not in the BIZ subset: they fall back to the Kana font.
    const std::string text = "操作方法：「Press」。あい";
    const Text::TextSpan span{0, static_cast<std::uint32_t>(text.size()), Text::TextStyle{fonts, 17.0f, {}}};
    Text::GlyphAtlas atlas(Text::AtlasOptions{128, 1, 4096});
    static_cast<void>(context.Rasterize(context.Layout(context.Shape(text, {&span, 1}), {}), atlas));
    std::vector<std::byte> pixels;
    for (std::size_t page = 0; page < atlas.PageCount(); ++page) {
        for (const std::uint8_t p : atlas.Page(page).pixels) {
            pixels.push_back(static_cast<std::byte>(p));
        }
    }
    MESSAGE("environment atlas " << Base::Sha256(pixels));
    CHECK(Base::Sha256(pixels) == std::string(GYO_TEXT_ENVIRONMENT_ATLAS));
}
