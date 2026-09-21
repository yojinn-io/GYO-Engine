#include <doctest/doctest.h>

#include "UiTestDocument.hpp"
#include "ui/UiDocumentCodec.hpp"
#include "ui/UiRuntime.hpp"

#include <memory>
#include <string>
#include <variant>

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

} // namespace

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

} // namespace Engine::Ui::Tests
