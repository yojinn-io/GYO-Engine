#include <doctest/doctest.h>
#include "AssertTestSupport.hpp"

#include "UiGoldenDocument.hpp"
#include "UiLayoutGolden.hpp"
#include "UiTestDocument.hpp"
#include "ui/UiDocumentCodec.hpp"
#include "ui/UiRuntime.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Engine::Ui::Tests {
namespace {

[[nodiscard]] std::shared_ptr<const UiDocument> Document() {
    auto parsed = UiDocumentCodec::Parse(kDocument);
    REQUIRE(parsed);
    return std::make_shared<const UiDocument>(std::move(parsed).value());
}

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

[[nodiscard]] UiEvaluatedElement Evaluated(
    std::string id,
    std::size_t drawOrder,
    std::array<float, 4> bounds,
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

[[nodiscard]] float Below(float value) noexcept {
    return std::nextafter(value, -std::numeric_limits<float>::infinity());
}

[[nodiscard]] std::string_view HitId(
    std::span<const UiEvaluatedElement> layout,
    float x,
    float y) {
    const UiEvaluatedElement* hit = HitTestUiLayout(layout, {x, y}, true);
    return hit == nullptr ? std::string_view{} : std::string_view{hit->id};
}

template <class Rect>
[[nodiscard]] LayoutGolden::Bits4 RectBits(const Rect& rect) noexcept {
    return {
        std::bit_cast<std::uint32_t>(rect.x),
        std::bit_cast<std::uint32_t>(rect.y),
        std::bit_cast<std::uint32_t>(rect.width),
        std::bit_cast<std::uint32_t>(rect.height),
    };
}

[[nodiscard]] std::array<std::uint32_t, 4> ColorBits(const UiColor& color) noexcept {
    return {
        std::bit_cast<std::uint32_t>(color.red),
        std::bit_cast<std::uint32_t>(color.green),
        std::bit_cast<std::uint32_t>(color.blue),
        std::bit_cast<std::uint32_t>(color.alpha),
    };
}

void CheckColor(const UiColor& actual, std::string_view expectedHex) {
    auto expected = DecodeSrgbHexColor(expectedHex);
    REQUIRE(expected);
    CHECK(ColorBits(actual) == ColorBits(expected.value()));
}

struct LayoutCase final {
    std::string_view name;
    std::string_view document;
    std::string_view canvas;
    UiViewport viewport;
    std::span<const LayoutGolden::LayoutRow> layout;
    std::span<const LayoutGolden::DrawRow> draws;
};

void CheckLayoutCase(const LayoutCase& testCase) {
    CAPTURE(testCase.name);
    auto parsed = UiDocumentCodec::Parse(testCase.document);
    REQUIRE(parsed);
    UiRuntime runtime;
    REQUIRE(runtime.Initialize(std::make_shared<const UiDocument>(std::move(parsed).value())));
    REQUIRE(runtime.ActivateCanvas(testCase.canvas));

    auto layout = runtime.EvaluatePreviewLayout(testCase.viewport);
    REQUIRE(layout);
    REQUIRE(layout.value().size() == testCase.layout.size());
    for (std::size_t index = 0; index < testCase.layout.size(); ++index) {
        CAPTURE(index);
        const UiEvaluatedElement& actual = layout.value()[index];
        const LayoutGolden::LayoutRow& expected = testCase.layout[index];
        CHECK(actual.id == expected.id);
        CHECK(actual.drawOrder == expected.drawOrder);
        CHECK(actual.listItemIndex.value_or(LayoutGolden::kNoItem) == expected.listItemIndex);
        CHECK(RectBits(actual.boundsPixels) == expected.boundsPixels);
        CHECK(RectBits(actual.clipPixels) == expected.clipPixels);
    }

    auto composed = runtime.ComposePreview(testCase.viewport);
    REQUIRE(composed);
    const std::vector<UiDrawCommand>& commands = composed.value().commands;
    REQUIRE(commands.size() == testCase.draws.size());
    for (std::size_t index = 0; index < testCase.draws.size(); ++index) {
        CAPTURE(index);
        const LayoutGolden::DrawRow& expected = testCase.draws[index];
        const UiDrawCommand& command = commands[index];
        switch (expected.kind) {
        case LayoutGolden::DrawKind::Quad: {
            const auto* quad = std::get_if<UiQuadDraw>(&command);
            REQUIRE(quad != nullptr);
            CHECK(RectBits(quad->destinationPixels) == expected.rect);
            CHECK(RectBits(quad->clipPixels) == expected.clipPixels);
            CheckColor(quad->color, expected.colorHex);
            break;
        }
        case LayoutGolden::DrawKind::Image: {
            const auto* image = std::get_if<UiImageDraw>(&command);
            REQUIRE(image != nullptr);
            CHECK(RectBits(image->destinationPixels) == expected.rect);
            CHECK(RectBits(image->clipPixels) == expected.clipPixels);
            CHECK(RectBits(image->sourceUv) == expected.sourceUv);
            CHECK(image->textureAssetId == expected.text);
            CheckColor(image->tint, expected.colorHex);
            break;
        }
        case LayoutGolden::DrawKind::Text: {
            const auto* text = std::get_if<UiTextDraw>(&command);
            REQUIRE(text != nullptr);
            CHECK(RectBits(text->boundsPixels) == expected.rect);
            CHECK(RectBits(text->clipPixels) == expected.clipPixels);
            CHECK(std::bit_cast<std::uint32_t>(text->pointSizePixels) == expected.pointSizePixels);
            CHECK(text->utf8 == expected.text);
            CheckColor(text->color, expected.colorHex);
            break;
        }
        }
    }
}

} // namespace

TEST_CASE("UiRuntime API misuse is a Programmer Error") {
    UiRuntime runtime;
    GYO_CHECK_ASSERTS(runtime.Initialize(nullptr));
    GYO_CHECK_ASSERTS(runtime.ActivateCanvas("screen"));
    GYO_CHECK_ASSERTS(runtime.ComposePreview({200.0F, 100.0F}));
    REQUIRE(runtime.Initialize(Document()));
    GYO_CHECK_ASSERTS(runtime.Compose(Bindings(), {200.0F, 100.0F}));
}

TEST_CASE("UiRuntime applies nested stretch layout and parent clipping") {
    UiRuntime runtime = Runtime();
    auto composed = runtime.ComposePreview({200.0F, 100.0F});
    REQUIRE(composed);

    bool foundStretchedButton = false;
    bool foundFirstRow = false;
    bool foundSecondRow = false;
    bool foundSelectedOutcome = false;
    for (const UiDrawCommand& command : composed.value().commands) {
        if (const auto* quad = std::get_if<UiQuadDraw>(&command);
            quad != nullptr && quad->destinationPixels.width == doctest::Approx(120.0F)) {
            foundStretchedButton = true;
            CHECK(quad->destinationPixels.x == doctest::Approx(40.0F));
            CHECK(quad->clipPixels.x == doctest::Approx(60.0F));
            CHECK(quad->clipPixels.width == doctest::Approx(80.0F));
        }
        if (const auto* text = std::get_if<UiTextDraw>(&command)) {
            foundFirstRow = foundFirstRow || text->utf8 == "A 2";
            foundSecondRow = foundSecondRow || text->utf8 == "B 5";
            foundSelectedOutcome = foundSelectedOutcome || text->utf8 == "WIN";
        }
    }
    CHECK(foundStretchedButton);
    CHECK(foundFirstRow);
    CHECK(foundSecondRow);
    CHECK(foundSelectedOutcome);
}

TEST_CASE("UiRuntime static pointer does not steal keyboard focus") {
    UiRuntime runtime = Runtime();
    UiInputFrame input{};
    input.pointerAvailable = true;
    input.pointerPixels = {120.0F, 10.0F};
    REQUIRE(runtime.Update(input, Bindings(), {200.0F, 100.0F}));
    CHECK(runtime.InteractionState().focusedElement == "button_one");

    REQUIRE(runtime.Update(input, Bindings(), {200.0F, 100.0F}));
    CHECK(runtime.InteractionState().focusedElement == "button_one");

    input.pointerPixels.x = 121.0F;
    REQUIRE(runtime.Update(input, Bindings(), {200.0F, 100.0F}));
    CHECK(runtime.InteractionState().focusedElement == "button_two");
}

TEST_CASE("UiRuntime reverse hit test honors the nested clip rectangle") {
    UiRuntime runtime = Runtime();
    UiInputFrame outsideClip{};
    outsideClip.pointerAvailable = true;
    outsideClip.pointerPrimaryPressed = true;
    outsideClip.pointerPixels = {55.0F, 40.0F};
    auto outside = runtime.Update(outsideClip, Bindings(), {200.0F, 100.0F});
    REQUIRE(outside);
    CHECK(outside.value().empty());

    UiInputFrame insideClip = outsideClip;
    insideClip.pointerPixels = {65.0F, 40.0F};
    auto inside = runtime.Update(insideClip, Bindings(), {200.0F, 100.0F});
    REQUIRE(inside);
    REQUIRE(inside.value().size() == 1);
    CHECK(inside.value()[0].action == "one");
    CHECK(std::holds_alternative<std::monostate>(inside.value()[0].payload));
}

TEST_CASE("shared evaluated layout exposes draw order, clipping, and list instances") {
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

TEST_CASE("UiRuntime wraps focus and quantizes typed slider actions") {
    UiRuntime runtime = Runtime();
    UiInputFrame next{};
    next.focusNextPressed = true;
    REQUIRE(runtime.Update(next, Bindings(), {100.0F, 100.0F}));
    REQUIRE(runtime.Update(next, Bindings(), {100.0F, 100.0F}));
    CHECK(runtime.InteractionState().focusedElement == "gamma_slider");

    UiInputFrame adjust{};
    adjust.adjustNextPressed = true;
    auto adjusted = runtime.Update(adjust, Bindings(), {100.0F, 100.0F});
    REQUIRE(adjusted);
    REQUIRE(adjusted.value().size() == 1);
    CHECK(adjusted.value()[0].action == "set_gamma");
    CHECK(std::get<double>(adjusted.value()[0].payload) == doctest::Approx(1.05));

    UiInputFrame pointer{};
    pointer.pointerAvailable = true;
    pointer.pointerPrimaryPressed = true;
    pointer.pointerPixels = {89.0F, 80.0F};
    auto maximum = runtime.Update(pointer, Bindings(), {100.0F, 100.0F});
    REQUIRE(maximum);
    REQUIRE(maximum.value().size() == 1);
    CHECK(std::get<double>(maximum.value()[0].payload) == doctest::Approx(1.5));
    CHECK(runtime.InteractionState().capturedElement == "gamma_slider");

    UiInputFrame release{};
    release.pointerAvailable = true;
    release.pointerPrimaryReleased = true;
    release.pointerPixels = pointer.pointerPixels;
    REQUIRE(runtime.Update(release, Bindings(), {100.0F, 100.0F}));
    CHECK(runtime.InteractionState().capturedElement.empty());
}

TEST_CASE("UiRuntime gives cancel priority and never falls back to previews") {
    UiRuntime runtime = Runtime();
    UiInputFrame input{};
    input.cancelPressed = true;
    input.activatePressed = true;
    auto events = runtime.Update(input, Bindings(), {100.0F, 100.0F});
    REQUIRE(events);
    REQUIRE(events.value().size() == 1);
    CHECK(events.value()[0].action == "back");

    auto missing = runtime.Compose({}, {100.0F, 100.0F});
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == UiErrorCode::MissingBinding);
}

TEST_CASE("HitTestUiLayout uses half-open bounds and clip rectangles") {
    SUBCASE("bounds: left and top edges hit, right and bottom edges miss") {
        const std::vector<UiEvaluatedElement> layout{
            Evaluated("box", 0, {10.0F, 20.0F, 30.0F, 40.0F}, {0.0F, 0.0F, 1000.0F, 1000.0F}),
        };
        CHECK(HitId(layout, 10.0F, 20.0F) == "box");
        CHECK(HitId(layout, 10.0F, 59.5F) == "box");
        CHECK(HitId(layout, 39.5F, 20.0F) == "box");
        CHECK(HitId(layout, Below(40.0F), Below(60.0F)) == "box");
        CHECK(HitId(layout, 40.0F, 30.0F).empty());
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

TEST_CASE("UiRuntime layout and draw list match exact master geometry") {
    using namespace LayoutGolden;
    const LayoutCase cases[]{
        {"kDocument 200x100", kDocument, "screen", {200.0F, 100.0F},
         kTestDocument200x100Layout, kTestDocument200x100Draws},
        {"kDocument 1920x1080", kDocument, "screen", {1920.0F, 1080.0F},
         kTestDocument1920x1080Layout, kTestDocument1920x1080Draws},
        {"kDocument 1000x333", kDocument, "screen", {1000.0F, 333.0F},
         kTestDocument1000x333Layout, kTestDocument1000x333Draws},
        {"golden 200x100", kGoldenSourceDocument, "main", {200.0F, 100.0F},
         kGolden200x100Layout, kGolden200x100Draws},
        {"golden 1920x1080", kGoldenSourceDocument, "main", {1920.0F, 1080.0F},
         kGolden1920x1080Layout, kGolden1920x1080Draws},
        {"golden 1000x333", kGoldenSourceDocument, "main", {1000.0F, 333.0F},
         kGolden1000x333Layout, kGolden1000x333Draws},
    };
    for (const LayoutCase& testCase : cases) {
        CheckLayoutCase(testCase);
    }
}

} // namespace Engine::Ui::Tests

TEST_CASE("shared UI layout rules: canvas fit, half-open containment and text alignment") {
    using namespace Engine::Ui;
    // Pillarbox: the height limits the scale and the canvas is centred.
    const UiCanvasFit fit = FitDesignCanvas({1920.0F, 1080.0F}, {1000.0F, 1000.0F});
    CHECK(fit.scale == 1000.0F / 1920.0F);
    CHECK(fit.offset.x == (1000.0F - 1920.0F * fit.scale) * 0.5F);
    CHECK(fit.offset.y == (1000.0F - 1080.0F * fit.scale) * 0.5F);

    const Engine::Math::Rect rect{10.0F, 20.0F, 30.0F, 40.0F};
    CHECK(ContainsUiPoint(rect, {10.0F, 20.0F}));
    CHECK_FALSE(ContainsUiPoint(rect, {40.0F, 30.0F}));   // right edge excluded
    CHECK_FALSE(ContainsUiPoint(rect, {20.0F, 60.0F}));   // bottom edge excluded
    CHECK_FALSE(ContainsUiPoint({5.0F, 5.0F, 0.0F, 10.0F}, {5.0F, 6.0F}));  // empty never hit

    const Engine::Math::Rect bounds{100.0F, 50.0F, 200.0F, 80.0F};
    const Engine::Math::Vec2 extent{40.0F, 10.0F};
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
