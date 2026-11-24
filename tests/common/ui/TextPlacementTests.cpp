// Text in a gyo.ui document laid out with the text Module (docs/architecture/target/v2/ui.zh-Hant.md, section 4.4):
// glyphs placed in their bounds by the document's alignment, at point_size times the canvas fit, on whole pixels, in
// the atlas; a text field's caret from a press, its composition's underline and caret, and the edit area for the
// input method. A real font (BIZ UDPGothic subset). Tags "[ui-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "UiTestSupport.hpp"

namespace Ui = GYO::Engine::Ui;
using GYO::Test::UiSupport::All;
using GYO::Test::UiSupport::Runtime;
using GYO::Test::UiSupport::TextWithFont;

namespace {

// A 100 x 50 design canvas: a centred text in a 100 x 20 box at the top (a quarter unit in, so its centred origin
// falls between pixels at the fit of 2), a text field (80 x 20 at y 25) and a
// button.
constexpr std::string_view kDocument = R"json({
  "schema": "gyo.ui", "version": 2,
  "design_canvas": {"size": [100, 50], "scale_mode": "fit"},
  "fonts": {"ui": "test.font"}, "default_font": "ui",
  "colors": {"bg": "#00000000", "white": "#FFFFFFFF", "gray": "#808080FF", "n": "#202020FF", "f": "#404040FF", "p": "#606060FF"},
  "actions": [{"id": "submit", "payload": "string"}, {"id": "go", "payload": "none"}],
  "bindings": [{"id": "name", "type": "string", "preview": ""}],
  "canvases": [{
    "id": "main", "backdrop_color": "bg", "focus_order": ["field", "go"],
    "children": [
      {"type": "text", "id": "title", "rect": {"anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0], "position": [0.25, 0.25], "size_delta": [100, 20]},
       "text": {"literal": "がっこう"}, "point_size": 10, "text_color": {"color": "white"}, "horizontal_align": "center", "vertical_align": "center"},
      {"type": "text_field", "id": "field", "rect": {"anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0], "position": [10, 25], "size_delta": [80, 20]},
       "binding": "name", "max_length": 16, "placeholder": "name", "placeholder_color": "gray", "point_size": 10,
       "text_color": {"color": "white"}, "vertical_align": "center", "action": "submit", "background": {"normal": "n", "focused": "f", "pressed": "p"}},
      {"type": "button", "id": "go", "rect": {"anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0], "position": [0, 46], "size_delta": [10, 4]},
       "text": {"literal": "Go"}, "text_color": {"color": "white"}, "action": "go", "background": {"normal": "n"}}
    ]
  }]
})json";

// Twice the design size: the fit scale is 2.
constexpr Ui::UiViewport kViewport{200.0F, 100.0F};

Ui::UiBindingTable Bindings(std::string name = "がっこう") { return {{"name", std::move(name)}}; }

bool Whole(const float v) { return v == std::floor(v); }

// The glyphs that lie inside a box (their rectangles' centres).
std::vector<Ui::UiGlyphDraw> GlyphsIn(const Ui::UiDrawList& list, const GYO::Engine::Math::Rect box) {
    std::vector<Ui::UiGlyphDraw> out;
    for (const Ui::UiGlyphDraw& g : All<Ui::UiGlyphDraw>(list)) {
        const float cx = g.rect.x + g.rect.width * 0.5F;
        const float cy = g.rect.y + g.rect.height * 0.5F;
        if (cx >= box.x && cx < box.x + box.width && cy >= box.y && cy < box.y + box.height) out.push_back(g);
    }
    return out;
}

Ui::UiInput PressAt(const float x, const float y) {
    Ui::UiInput input;
    input.pointerAvailable = true;
    input.pointerPixels = {x, y};
    input.pointerPrimaryPressed = true;
    input.pointerPrimaryHeld = true;
    return input;
}

} // namespace

TEST_CASE("ui_layout: a text's glyphs are laid out at point_size times the fit, centred in its box, on whole pixels") {
    Ui::UiRuntime runtime = Runtime(kDocument, "main");
    Ui::UiText text = TextWithFont();
    auto frame = runtime.Compose(Bindings(), kViewport, text);
    REQUIRE(frame);
    const std::vector<Ui::UiGlyphDraw> title = GlyphsIn(frame.value().drawList, {0.0F, 0.0F, 200.0F, 40.0F});
    REQUIRE(title.size() == 4);
    float left = title.front().rect.x;
    float right = 0.0F;
    float top = title.front().rect.y;
    float bottom = 0.0F;
    for (const Ui::UiGlyphDraw& g : title) {
        CHECK_MESSAGE((Whole(g.rect.x) && Whole(g.rect.y)), "[ui-glyph-whole-pixels]");
        left = std::min(left, g.rect.x);
        right = std::max(right, g.rect.x + g.rect.width);
        top = std::min(top, g.rect.y);
        bottom = std::max(bottom, g.rect.y + g.rect.height);
    }
    // Centred in the 200 x 40 box (the 100 x 20 design box times 2), within the glyphs' side bearings.
    CHECK_MESSAGE(std::abs((left + right) * 0.5F - 100.0F) <= 3.0F, "[ui-text-centred] x " << left << ".." << right);
    CHECK_MESSAGE(std::abs((top + bottom) * 0.5F - 20.0F) <= 4.0F, "[ui-text-centred] y " << top << ".." << bottom);
    // 20 px per em: four full-width kana are about 80 px wide (not 40, the unscaled size).
    CHECK_MESSAGE((right - left > 60.0F && right - left <= 84.0F), "[ui-text-scaled] width " << right - left);
    // Each glyph is in the atlas where its draw says, with ink there.
    const GYO::Engine::Text::GlyphAtlas& atlas = text.Atlas();
    REQUIRE(atlas.PageCount() >= 1);
    for (const Ui::UiGlyphDraw& g : title) {
        const auto page = atlas.Page(g.page);
        unsigned ink = 0;
        for (std::uint32_t y = 0; y < static_cast<std::uint32_t>(g.rect.height); ++y) {
            for (std::uint32_t x = 0; x < static_cast<std::uint32_t>(g.rect.width); ++x) {
                ink += page.pixels[(g.atlasY + y) * page.rowPitch + g.atlasX + x];
            }
        }
        CHECK_MESSAGE(ink > 0, "[ui-glyph-atlas-rect]");
    }
}

TEST_CASE("ui_layout: a text whose font is not in UiText is left out, and the rest is drawn") {
    Ui::UiRuntime runtime = Runtime(kDocument, "main");
    Ui::UiText empty;
    auto frame = runtime.Compose(Bindings(), kViewport, empty);
    REQUIRE(frame);
    CHECK(All<Ui::UiGlyphDraw>(frame.value().drawList).empty());
    // Backdrop, the field's background and the button's.
    CHECK(All<Ui::UiQuadDraw>(frame.value().drawList).size() == 3);
}

TEST_CASE("ui_text_field: a press puts the caret between the glyphs it falls between") {
    Ui::UiRuntime runtime = Runtime(kDocument, "main");
    Ui::UiText text = TextWithFont();
    // Where the third glyph of the field's text starts: press just left of it.
    auto shown = runtime.Compose(Bindings(), kViewport, text);
    REQUIRE(shown);
    std::vector<Ui::UiGlyphDraw> glyphs = GlyphsIn(shown.value().drawList, {20.0F, 50.0F, 160.0F, 40.0F});
    REQUIRE(glyphs.size() == 4);
    std::sort(glyphs.begin(), glyphs.end(), [](const auto& a, const auto& b) { return a.rect.x < b.rect.x; });
    const float between = (glyphs[1].rect.x + glyphs[1].rect.width + glyphs[2].rect.x) * 0.5F;
    REQUIRE(runtime.Update(PressAt(between, 70.0F), Bindings(), kViewport, text));
    CHECK(runtime.InteractionState().editingElement == "field");
    Ui::UiInput type;
    type.text.push_back({Ui::UiTextInputKind::Insert, "X", -1});
    type.activatePressed = true;
    auto submitted = runtime.Update(type, Bindings(), kViewport, text);
    REQUIRE(submitted);
    REQUIRE(submitted.value().size() == 1);
    CHECK_MESSAGE(std::get<std::string>(submitted.value()[0].payload) == "がっXこう", "[ui-press-caret]");
}

TEST_CASE("ui_text_field: a composition is drawn underlined with the caret in it, and the edit area follows the caret") {
    Ui::UiRuntime runtime = Runtime(kDocument, "main");
    Ui::UiText text = TextWithFont();
    const Ui::UiBindingTable empty = Bindings("");
    Ui::UiInput activate;
    activate.activatePressed = true;
    REQUIRE(runtime.Update(activate, empty, kViewport, text));
    REQUIRE(runtime.InteractionState().editingElement == "field");

    auto before = runtime.Compose(empty, kViewport, text);
    REQUIRE(before);
    REQUIRE(before.value().editArea);
    CHECK_MESSAGE(before.value().editArea->field.x == 20.0F, "[ui-edit-area]");
    CHECK(before.value().editArea->field.width == 160.0F);
    const float emptyCaret = before.value().editArea->caretX;

    Ui::UiInput compose;
    compose.text.push_back({Ui::UiTextInputKind::Composition, "がっこう", 2});
    REQUIRE(runtime.Update(compose, empty, kViewport, text));
    auto frame = runtime.Compose(empty, kViewport, text);
    REQUIRE(frame);
    const std::vector<Ui::UiGlyphDraw> glyphs = GlyphsIn(frame.value().drawList, {20.0F, 50.0F, 160.0F, 40.0F});
    REQUIRE(glyphs.size() == 4);
    float glyphLeft = glyphs.front().rect.x;
    float glyphRight = 0.0F;
    for (const auto& g : glyphs) {
        glyphLeft = std::min(glyphLeft, g.rect.x);
        glyphRight = std::max(glyphRight, g.rect.x + g.rect.width);
    }
    // The underline: a thin quad (1 px at 20 px per em) under the four glyphs; the caret: a quad after the second.
    const std::vector<Ui::UiQuadDraw> quads = All<Ui::UiQuadDraw>(frame.value().drawList);
    bool underline = false;
    for (const auto& q : quads) {
        if (q.rect.height == 1.0F && q.rect.x <= glyphLeft + 2.0F && q.rect.x + q.rect.width >= glyphRight - 2.0F) underline = true;
    }
    CHECK_MESSAGE(underline, "[ui-composition-underline]");
    REQUIRE(frame.value().editArea);
    const float caret = frame.value().editArea->caretX;
    // Cursor 2 (UTF-16 units): after "がっ", about half way through the text.
    CHECK_MESSAGE((caret > emptyCaret + 20.0F && caret < emptyCaret + 60.0F), "[ui-composition-caret] " << caret);
    // The commit ends the composition and replaces it; the activate submits the committed text (UI-5).
    Ui::UiInput commit;
    commit.text.push_back({Ui::UiTextInputKind::Insert, "学校", -1});
    commit.activatePressed = true;
    auto submitted = runtime.Update(commit, empty, kViewport, text);
    REQUIRE(submitted);
    REQUIRE(submitted.value().size() == 1);
    CHECK(std::get<std::string>(submitted.value()[0].payload) == "学校");
}

TEST_CASE("ui_text_field: UTF-16 offsets count a code point outside the Basic Multilingual Plane twice") {
    CHECK(Ui::UiUtf16Offset("abc", 2) == 2);
    CHECK(Ui::UiUtf16Offset("がっこう", 2) == 6);
    // U+1F600 is four bytes of UTF-8 and two UTF-16 units.
    const std::string emoji = "a\xF0\x9F\x98\x80" "b";
    CHECK_MESSAGE(Ui::UiUtf16Offset(emoji, 3) == 5, "[ui-utf16-surrogates]");
    CHECK(Ui::UiUtf16Offset(emoji, 2) == 1);
    CHECK(Ui::UiUtf16Offset(emoji, 99) == emoji.size());
}
