// The gyo.ui runtime (docs/architecture/target/v2/ui.zh-Hant.md, section 4.5; UI-3): ported from master's
// tests/common/ui/UiRuntimeTests.cpp. Text is no longer a draw command: a text's evaluation (compose, select, list
// items) is checked by the glyphs drawn in its bounds with the test font. Tags "[ui-...]" are the mutants'
// expectations.
#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "AssertTestSupport.hpp"
#include "UiTestSupport.hpp"

namespace GYO::Engine::Ui {
namespace {

constexpr std::string_view kDocument = R"json({
  "schema": "gyo.ui",
  "version": 1,
  "design_canvas": { "size": [100, 100], "scale_mode": "fit" },
  "fonts": { "ui": "test.font" },
  "default_font": "ui",
  "colors": {
    "background": "#000000FF",
    "white": "#FFFFFFFF",
    "gray": "#808080FF",
    "normal": "#202020FF",
    "focused": "#804000FF",
    "pressed": "#C06000FF",
    "track": "#303030FF",
    "fill": "#E08020FF",
    "thumb": "#FFFFFFFF"
  },
  "actions": [
    { "id": "back", "payload": "none" },
    { "id": "one", "payload": "none" },
    { "id": "two", "payload": "none" },
    { "id": "set_gamma", "payload": "number" }
  ],
  "bindings": [
    { "id": "gamma", "type": "number", "preview": 1.0 },
    { "id": "outcome", "type": "enum", "values": ["win", "lose"], "preview": "win" },
    {
      "id": "rooms",
      "type": "list<object>",
      "item_fields": { "kills": "integer", "name": "string" },
      "preview": [
        { "kills": 2, "name": "A" },
        { "kills": 5, "name": "B" }
      ]
    }
  ],
  "canvases": [
    {
      "id": "screen",
      "backdrop_color": "background",
      "default_focus": "button_one",
      "cancel_action": "back",
      "focus_order": ["button_one", "button_two", "gamma_slider"],
      "children": [
        {
          "type": "container",
          "id": "stretch_parent",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [1, 1], "pivot": [0.5, 0.5],
            "position": [0, 0], "size_delta": [-20, -20]
          },
          "children": [
            {
              "type": "button",
              "id": "button_one",
              "rect": {
                "anchor_min": [0, 0], "anchor_max": [1, 1], "pivot": [0.5, 0.5],
                "position": [0, 0], "size_delta": [40, -40]
              },
              "text": { "literal": "ゲームスタート" },
              "point_size": 10,
              "text_color": { "color": "white" },
              "horizontal_align": "center",
              "vertical_align": "center",
              "action": "one",
              "background": { "normal": "normal", "focused": "focused", "pressed": "pressed" }
            }
          ]
        },
        {
          "type": "button",
          "id": "button_two",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0],
            "position": [65, 5], "size_delta": [30, 20]
          },
          "text": { "literal": "TWO" },
          "point_size": 10,
          "text_color": { "color": "white" },
          "action": "two",
          "background": { "normal": "normal", "focused": "focused", "pressed": "pressed" }
        },
        {
          "type": "horizontal_slider",
          "id": "gamma_slider",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0],
            "position": [10, 75], "size_delta": [80, 20]
          },
          "text": { "literal": "GAMMA" },
          "point_size": 8,
          "text_color": { "color": "white" },
          "action": "set_gamma",
          "background": { "normal": "normal", "focused": "focused", "pressed": "pressed" },
          "binding": "gamma",
          "minimum": 0.75,
          "maximum": 1.5,
          "step": 0.05,
          "value_format": { "decimals": 2, "show_plus": false },
          "track_color": "track",
          "fill_color": "fill",
          "thumb_color": "thumb"
        },
        {
          "type": "fixed_step_list",
          "id": "room_list",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0],
            "position": [5, 2], "size_delta": [40, 20]
          },
          "binding": "rooms",
          "max_items": 4,
          "item_step": [0, 9],
          "template": [
            {
              "type": "text",
              "id": "room_row",
              "rect": {
                "anchor_min": [0, 0], "anchor_max": [1, 0], "pivot": [0, 0],
                "position": [0, 0], "size_delta": [0, 8]
              },
              "text": {
                "compose": {
                  "format": "{name} {kills}",
                  "placeholders": {
                    "kills": { "item_field": "kills", "format": { "decimals": 0, "show_plus": false } },
                    "name": { "item_field": "name" }
                  }
                }
              },
              "point_size": 7,
              "text_color": { "color": "gray" }
            }
          ]
        },
        {
          "type": "text",
          "id": "outcome",
          "rect": {
            "anchor_min": [0.5, 0.5], "anchor_max": [0.5, 0.5], "pivot": [0.5, 0.5],
            "position": [0, 0], "size_delta": [30, 10]
          },
          "text": {
            "select": {
              "binding": "outcome",
              "cases": { "lose": "LOSE", "win": "WIN" }
            }
          },
          "point_size": 8,
          "text_color": { "color": "white" },
          "horizontal_align": "center",
          "vertical_align": "center"
        }
      ]
    }
  ]
})json";

// A UiText without fonts (text left out) for the cases that do not look at text.
UiText& Text() {
    static UiText text;
    return text;
}

[[nodiscard]] std::shared_ptr<const UiDocument> Document() { return GYO::Test::UiSupport::Parse(kDocument); }

[[nodiscard]] UiBindingTable Bindings() {
    UiList rooms;
    rooms.items.push_back({{{"kills", std::int64_t{2}}, {"name", std::string{"A"}}}});
    rooms.items.push_back({{{"kills", std::int64_t{5}}, {"name", std::string{"B"}}}});
    return {
        {"gamma", 1.0},
        {"outcome", std::string{"win"}},
        {"rooms", std::move(rooms)},
    };
}

[[nodiscard]] UiRuntime Runtime() {
    UiRuntime runtime;
    REQUIRE(runtime.Initialize(Document()));
    REQUIRE(runtime.ActivateCanvas("screen"));
    return runtime;
}

[[nodiscard]] UiEvaluatedElement Evaluated(std::string id, std::size_t drawOrder, std::array<float, 4> bounds,
                                           std::array<float, 4> clip) {
    UiEvaluatedElement element;
    element.id = std::move(id);
    element.type = UiElementType::Button;
    element.boundsPixels = {bounds[0], bounds[1], bounds[2], bounds[3]};
    element.clipPixels = {clip[0], clip[1], clip[2], clip[3]};
    element.drawOrder = drawOrder;
    element.interactive = true;
    return element;
}

[[nodiscard]] float Below(float value) noexcept { return std::nextafter(value, -std::numeric_limits<float>::infinity()); }

[[nodiscard]] std::string_view HitId(std::span<const UiEvaluatedElement> layout, float x, float y) {
    const UiEvaluatedElement* hit = HitTestUiLayout(layout, {x, y}, true);
    return hit == nullptr ? std::string_view{} : std::string_view{hit->id};
}

// The glyphs drawn inside an evaluated element's bounds.
std::size_t GlyphsIn(const UiDrawList& list, const Math::Rect bounds) {
    std::size_t n = 0;
    for (const UiGlyphDraw& g : GYO::Test::UiSupport::All<UiGlyphDraw>(list)) {
        const float cx = g.rect.x + g.rect.width * 0.5F;
        const float cy = g.rect.y + g.rect.height * 0.5F;
        if (cx >= bounds.x && cx < bounds.x + bounds.width && cy >= bounds.y && cy < bounds.y + bounds.height) ++n;
    }
    return n;
}

} // namespace

TEST_CASE("ui_runtime: compose and select text and list items are evaluated with the bindings") {
    UiRuntime runtime = Runtime();
    UiText text = GYO::Test::UiSupport::TextWithFont();
    const UiViewport viewport{1000.0F, 1000.0F};
    auto layout = runtime.EvaluateLayout(Bindings(), viewport);
    REQUIRE(layout);
    auto frame = runtime.Compose(Bindings(), viewport, text);
    REQUIRE(frame);
    std::vector<std::size_t> rows;
    std::size_t outcome = 0;
    for (const UiEvaluatedElement& element : layout.value()) {
        if (element.id == "room_row") rows.push_back(GlyphsIn(frame.value().drawList, element.boundsPixels));
        if (element.id == "outcome") outcome = GlyphsIn(frame.value().drawList, element.boundsPixels);
    }
    // "A 2" and "B 5": two glyphs each (a space draws nothing); "WIN": three.
    REQUIRE(rows.size() == 2);
    CHECK_MESSAGE((rows[0] == 2 && rows[1] == 2), "[ui-list-items] " << rows[0] << " " << rows[1]);
    CHECK_MESSAGE(outcome == 3, "[ui-select-text] " << outcome);
    UiBindingTable lose = Bindings();
    lose["outcome"] = std::string{"lose"};
    auto losing = runtime.Compose(lose, viewport, text);
    REQUIRE(losing);
    for (const UiEvaluatedElement& element : layout.value()) {
        if (element.id == "outcome") CHECK_MESSAGE(GlyphsIn(losing.value().drawList, element.boundsPixels) == 4, "[ui-select-text] LOSE");
    }
}

TEST_CASE("ui_runtime: UiRuntime API misuse is a Programmer Error") {
    UiRuntime runtime;
    GYO_CHECK_ASSERTS(runtime.Initialize(nullptr));
    GYO_CHECK_ASSERTS(runtime.ActivateCanvas("screen"));
    GYO_CHECK_ASSERTS(runtime.ComposePreview({200.0F, 100.0F}, Text()));
    REQUIRE(runtime.Initialize(Document()));
    GYO_CHECK_ASSERTS(runtime.Compose(Bindings(), {200.0F, 100.0F}, Text()));
}

TEST_CASE("ui_runtime: UiRuntime applies nested stretch layout and parent clipping") {
    UiRuntime runtime = Runtime();
    auto composed = runtime.ComposePreview({200.0F, 100.0F}, Text());
    REQUIRE(composed);

    // The rows and the outcome text are checked with a font in "compose and select text ..." above.
    bool foundStretchedButton = false;
    for (const UiDrawCommand& command : composed.value().drawList.commands) {
        if (const auto* quad = std::get_if<UiQuadDraw>(&command);
            quad != nullptr && quad->rect.width == doctest::Approx(120.0F)) {
            foundStretchedButton = true;
            CHECK(quad->rect.x == doctest::Approx(40.0F));
            CHECK_MESSAGE(quad->clip.x == doctest::Approx(60.0F), "[ui-parent-clip]");
            CHECK(quad->clip.width == doctest::Approx(80.0F));
        }
    }
    CHECK(foundStretchedButton);
}

TEST_CASE("ui_runtime: UiRuntime static pointer does not steal keyboard focus") {
    UiRuntime runtime = Runtime();
    UiInput input{};
    input.pointerAvailable = true;
    input.pointerPixels = {120.0F, 10.0F};
    REQUIRE(runtime.Update(input, Bindings(), {200.0F, 100.0F}, Text()));
    CHECK(runtime.InteractionState().focusedElement == "button_one");

    REQUIRE(runtime.Update(input, Bindings(), {200.0F, 100.0F}, Text()));
    CHECK(runtime.InteractionState().focusedElement == "button_one");

    input.pointerPixels.x = 121.0F;
    REQUIRE(runtime.Update(input, Bindings(), {200.0F, 100.0F}, Text()));
    CHECK(runtime.InteractionState().focusedElement == "button_two");
}

TEST_CASE("ui_runtime: UiRuntime reverse hit test honors the nested clip rectangle") {
    UiRuntime runtime = Runtime();
    UiInput outsideClip{};
    outsideClip.pointerAvailable = true;
    outsideClip.pointerPrimaryPressed = true;
    outsideClip.pointerPixels = {55.0F, 40.0F};
    auto outside = runtime.Update(outsideClip, Bindings(), {200.0F, 100.0F}, Text());
    REQUIRE(outside);
    CHECK(outside.value().empty());

    UiInput insideClip = outsideClip;
    insideClip.pointerPixels = {65.0F, 40.0F};
    auto inside = runtime.Update(insideClip, Bindings(), {200.0F, 100.0F}, Text());
    REQUIRE(inside);
    REQUIRE(inside.value().size() == 1);
    CHECK(inside.value()[0].action == "one");
    CHECK(std::holds_alternative<std::monostate>(inside.value()[0].payload));
}

TEST_CASE("ui_runtime: shared evaluated layout exposes draw order, clipping, and list instances") {
    UiRuntime runtime = Runtime();
    auto layout = runtime.EvaluatePreviewLayout({200.0F, 100.0F});
    REQUIRE(layout);

    const UiEvaluatedElement* outside = HitTestUiLayout(
        layout.value(),
        {55.0F, 40.0F},
        true);
    CHECK(outside == nullptr);
    const UiEvaluatedElement* inside = HitTestUiLayout(
        layout.value(),
        {65.0F, 40.0F},
        true);
    REQUIRE(inside != nullptr);
    CHECK(inside->id == "button_one");
    CHECK(inside->clipPixels.x == doctest::Approx(60.0F));

    std::size_t rowInstances = 0;
    for (const UiEvaluatedElement& element : layout.value()) {
        if (element.id == "room_row") {
            REQUIRE(element.listItemIndex.has_value());
            CHECK(*element.listItemIndex == rowInstances);
            ++rowInstances;
        }
    }
    CHECK(rowInstances == 2);
}

TEST_CASE("ui_runtime: UiRuntime wraps focus and quantizes typed slider actions") {
    UiRuntime runtime = Runtime();
    UiInput next{};
    next.focusNextPressed = true;
    REQUIRE(runtime.Update(next, Bindings(), {100.0F, 100.0F}, Text()));
    REQUIRE(runtime.Update(next, Bindings(), {100.0F, 100.0F}, Text()));
    CHECK_MESSAGE(runtime.InteractionState().focusedElement == "gamma_slider", "[ui-focus-wrap]");

    UiInput adjust{};
    adjust.adjustNextPressed = true;
    auto adjusted = runtime.Update(adjust, Bindings(), {100.0F, 100.0F}, Text());
    REQUIRE(adjusted);
    REQUIRE(adjusted.value().size() == 1);
    CHECK(adjusted.value()[0].action == "set_gamma");
    CHECK_MESSAGE(std::get<double>(adjusted.value()[0].payload) == doctest::Approx(1.05), "[ui-slider-step]");

    UiInput pointer{};
    pointer.pointerAvailable = true;
    pointer.pointerPrimaryPressed = true;
    pointer.pointerPixels = {89.0F, 80.0F};
    auto maximum = runtime.Update(pointer, Bindings(), {100.0F, 100.0F}, Text());
    REQUIRE(maximum);
    REQUIRE(maximum.value().size() == 1);
    CHECK_MESSAGE(std::get<double>(maximum.value()[0].payload) == doctest::Approx(1.5), "[ui-slider-clamp]");
    CHECK(runtime.InteractionState().capturedElement == "gamma_slider");

    UiInput release{};
    release.pointerAvailable = true;
    release.pointerPrimaryReleased = true;
    release.pointerPixels = pointer.pointerPixels;
    REQUIRE(runtime.Update(release, Bindings(), {100.0F, 100.0F}, Text()));
    CHECK(runtime.InteractionState().capturedElement.empty());
}

TEST_CASE("ui_runtime: UiRuntime gives cancel priority and never falls back to previews") {
    UiRuntime runtime = Runtime();
    UiInput input{};
    input.cancelPressed = true;
    input.activatePressed = true;
    auto events = runtime.Update(input, Bindings(), {100.0F, 100.0F}, Text());
    REQUIRE(events);
    REQUIRE(events.value().size() == 1);
    CHECK_MESSAGE(events.value()[0].action == "back", "[ui-cancel-first]");

    auto missing = runtime.Compose({}, {100.0F, 100.0F}, Text());
    REQUIRE_FALSE(missing);
    CHECK_MESSAGE(missing.error().code == UiFailureCode::MissingBinding, "[ui-no-preview-fallback]");
}

TEST_CASE("ui_runtime: HitTestUiLayout uses half-open bounds and clip rectangles") {
    SUBCASE("bounds: left and top edges hit, right and bottom edges miss") {
        const std::vector<UiEvaluatedElement> layout{
            Evaluated("box", 0, {10.0F, 20.0F, 30.0F, 40.0F}, {0.0F, 0.0F, 1000.0F, 1000.0F}),
        };
        CHECK(HitId(layout, 10.0F, 20.0F) == "box");
        CHECK(HitId(layout, 10.0F, 59.5F) == "box");
        CHECK(HitId(layout, 39.5F, 20.0F) == "box");
        CHECK(HitId(layout, Below(40.0F), Below(60.0F)) == "box");
        CHECK_MESSAGE(HitId(layout, 40.0F, 30.0F).empty(), "[ui-half-open]");
        CHECK(HitId(layout, 20.0F, 60.0F).empty());
        CHECK(HitId(layout, 40.0F, 60.0F).empty());
        CHECK(HitId(layout, Below(10.0F), 30.0F).empty());
        CHECK(HitId(layout, 20.0F, Below(20.0F)).empty());
    }
    SUBCASE("clip: right and bottom clip edges miss inside the bounds") {
        const std::vector<UiEvaluatedElement> layout{
            Evaluated("clipped", 0, {0.0F, 0.0F, 100.0F, 100.0F}, {5.0F, 5.0F, 45.0F, 45.0F}),
        };
        CHECK(HitId(layout, 5.0F, 5.0F) == "clipped");
        CHECK(HitId(layout, Below(50.0F), Below(50.0F)) == "clipped");
        CHECK(HitId(layout, 50.0F, 10.0F).empty());
        CHECK(HitId(layout, 10.0F, 50.0F).empty());
        CHECK(HitId(layout, Below(5.0F), 10.0F).empty());
        CHECK(HitId(layout, 75.0F, 75.0F).empty());
    }
    SUBCASE("a fully clipped element is never hit") {
        const std::vector<UiEvaluatedElement> layout{
            Evaluated("zero_clip", 0, {0.0F, 0.0F, 100.0F, 100.0F}, {25.0F, 25.0F, 0.0F, 0.0F}),
            Evaluated("zero_width", 1, {0.0F, 0.0F, 100.0F, 100.0F}, {25.0F, 25.0F, 0.0F, 50.0F}),
            Evaluated("zero_height", 2, {0.0F, 0.0F, 100.0F, 100.0F}, {25.0F, 25.0F, 50.0F, 0.0F}),
        };
        for (const float x : {0.0F, 24.0F, 25.0F, 26.0F, 50.0F, 99.0F}) {
            for (const float y : {0.0F, 24.0F, 25.0F, 26.0F, 50.0F, 99.0F}) {
                CAPTURE(x);
                CAPTURE(y);
                CHECK(HitId(layout, x, y).empty());
            }
        }
    }
    SUBCASE("shared edges resolve to the element that owns the left or top edge") {
        const std::vector<UiEvaluatedElement> layout{
            Evaluated("left", 0, {0.0F, 0.0F, 50.0F, 50.0F}, {0.0F, 0.0F, 50.0F, 50.0F}),
            Evaluated("right", 1, {50.0F, 0.0F, 50.0F, 50.0F}, {50.0F, 0.0F, 50.0F, 50.0F}),
        };
        CHECK(HitId(layout, 50.0F, 10.0F) == "right");
        CHECK(HitId(layout, Below(50.0F), 10.0F) == "left");
        CHECK(HitId(layout, 100.0F, 10.0F).empty());
    }
}

} // namespace GYO::Engine::Ui

TEST_CASE("ui_runtime: shared UI layout rules: canvas fit, half-open containment and text alignment") {
    using namespace GYO::Engine::Ui;
    // Pillarbox: the height limits the scale and the canvas is centred.
    const UiCanvasFit fit = FitDesignCanvas({1920.0F, 1080.0F}, {1000.0F, 1000.0F});
    CHECK(fit.scale == 1000.0F / 1920.0F);
    CHECK(fit.offset.x == (1000.0F - 1920.0F * fit.scale) * 0.5F);
    CHECK(fit.offset.y == (1000.0F - 1080.0F * fit.scale) * 0.5F);

    const GYO::Engine::Math::Rect rect{10.0F, 20.0F, 30.0F, 40.0F};
    CHECK(ContainsUiPoint(rect, {10.0F, 20.0F}));
    CHECK_FALSE(ContainsUiPoint(rect, {40.0F, 30.0F}));   // right edge excluded
    CHECK_FALSE(ContainsUiPoint(rect, {20.0F, 60.0F}));   // bottom edge excluded
    CHECK_FALSE(ContainsUiPoint({5.0F, 5.0F, 0.0F, 10.0F}, {5.0F, 6.0F}));  // empty never hit

    const GYO::Engine::Math::Rect bounds{100.0F, 50.0F, 200.0F, 80.0F};
    const GYO::Engine::Math::Vec2 extent{40.0F, 10.0F};
    const auto topLeft = AlignUiText(bounds, extent, UiHorizontalAlign::Left, UiVerticalAlign::Top);
    CHECK(topLeft.x == 100.0F);
    CHECK(topLeft.y == 50.0F);
    const auto centred = AlignUiText(bounds, extent, UiHorizontalAlign::Center, UiVerticalAlign::Center);
    CHECK(centred.x == 180.0F);
    CHECK(centred.y == 85.0F);
    const auto far = AlignUiText(bounds, extent, UiHorizontalAlign::Right, UiVerticalAlign::Bottom);
    CHECK(far.x == 260.0F);
    CHECK(far.y == 120.0F);
}
