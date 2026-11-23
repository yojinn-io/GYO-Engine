#include "doctest/doctest.h"

#include <cstdlib>
#include <string>

#include "TextTestSupport.hpp"
#include "engine/base/Sha256.hpp"

// Design Invariants TX-7 and TX-8: the environment does not change shaping or
// rasterizing. This executable sets HB_SHAPER_LIST=fallback (HarfBuzz's shaper
// without OpenType features), HB_OPTIONS and FREETYPE_PROPERTIES (other
// hinting choices) before it uses HarfBuzz or FreeType at all; HarfBuzz is
// built with HB_NO_GETENV and FreeType without
// FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES, so the kerning and the ligature are
// still there and the bitmaps are the ones without the variables.

namespace {

void Set(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

} // namespace

using namespace GYO::Engine;
using GYO::Test::Text::Fixture;
using GYO::Test::Text::Units16;

TEST_CASE("text environment: HarfBuzz and FreeType ignore the environment") {
    Set("HB_SHAPER_LIST", "fallback");
    Set("HB_OPTIONS", "uniscribe-bug-compatible");
    Set("FREETYPE_PROPERTIES",
        "truetype:interpreter-version=35 cff:no-stem-darkening=0 cff:hinting-engine=freetype "
        "autofitter:no-stem-darkening=0 autofitter:fallback-script=none autofitter:default-script=none");
    REQUIRE(std::string(std::getenv("HB_SHAPER_LIST")) == "fallback");
    Fixture f;
    const Text::TextLayout layout = f.Lay("AVfi", f.Style(f.latinFirst));
    REQUIRE(layout.glyphs.size() == 3);
    CHECK(layout.glyphs[0].advance == Units16(512 - 128));
    CHECK(layout.glyphs[2].advance == Units16(600));
    Text::TextContext context;
    const Text::FontId biz = context.AddFont(GYO::Test::Text::LoadFont("BIZUDPGothic-Subset.ttf"), 0).value();
    const std::vector<Text::FontId> fonts{biz};
    const std::string text = "操作方法：「A」。";
    const Text::TextSpan span{0, static_cast<std::uint32_t>(text.size()), Text::TextStyle{fonts, 17.0f, {}}};
    Text::GlyphAtlas atlas(Text::AtlasOptions{128, 1, 4096});
    static_cast<void>(context.Rasterize(context.Layout(context.Shape(text, {&span, 1}), {}), atlas));
    std::vector<std::byte> pixels;
    for (const std::uint8_t p : atlas.Page(0).pixels) {
        pixels.push_back(static_cast<std::byte>(p));
    }
    MESSAGE("environment atlas " << Base::Sha256(pixels));
    CHECK(Base::Sha256(pixels) == std::string(GYO_TEXT_ENVIRONMENT_ATLAS));
}
