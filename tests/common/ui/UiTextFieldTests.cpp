#include <doctest/doctest.h>

#include "engine/ui/UiDocumentCodec.hpp"
#include "engine/ui/UiRuntime.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// v8-tool-ui-text-field: gyo.ui version 2's text_field, and the Ui Design
// Invariants 4-6 of docs/ui_toolchain.md that it carries.
namespace Engine::Ui::Tests {
namespace {

// A 100x100 design canvas drawn 1:1: the field is 80x20 at the top left, the
// button 80x20 below it.
constexpr std::string_view kTextFieldDocument = R"json({
  "schema": "gyo.ui",
  "version": 2,
  "design_canvas": { "size": [100, 100], "scale_mode": "fit" },
  "fonts": { "ui": "test.font" },
  "default_font": "ui",
  "colors": {
    "background": "#000000FF",
    "white": "#FFFFFFFF",
    "gray": "#808080FF",
    "normal": "#202020FF",
    "focused": "#804000FF",
    "editing": "#C06000FF"
  },
  "actions": [
    { "id": "submit_address", "payload": "string" },
    { "id": "go", "payload": "none" },
    { "id": "back", "payload": "none" }
  ],
  "bindings": [
    { "id": "address", "type": "string", "preview": "" }
  ],
  "canvases": [
    {
      "id": "main",
      "backdrop_color": "background",
      "cancel_action": "back",
      "focus_order": ["address_field", "go_button"],
      "children": [
        {
          "type": "text_field",
          "id": "address_field",
          "rect": { "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0], "position": [0, 0], "size_delta": [80, 20] },
          "point_size": 10,
          "text_color": { "color": "white" },
          "action": "submit_address",
          "background": { "normal": "normal", "focused": "focused", "pressed": "editing" },
          "binding": "address",
          "max_length": 8,
          "placeholder": "host:port",
          "placeholder_color": "gray"
        },
        {
          "type": "button",
          "id": "go_button",
          "rect": { "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0], "position": [0, 50], "size_delta": [80, 20] },
          "text": { "literal": "Go" },
          "text_color": { "color": "white" },
          "action": "go",
          "background": { "normal": "normal" }
        }
      ]
    }
  ]
})json";

constexpr UiViewport kViewport{100.0F, 100.0F};

[[nodiscard]] std::string Replace(std::string text, std::string_view from, std::string_view to) {
    const std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
    return text;
}

[[nodiscard]] UiRuntime Runtime() {
    auto parsed = UiDocumentCodec::Parse(kTextFieldDocument, "text-field-test");
    REQUIRE(parsed);
    UiRuntime runtime;
    REQUIRE(runtime.Initialize(std::make_shared<const UiDocument>(std::move(parsed).value())));
    REQUIRE(runtime.ActivateCanvas("main"));
    return runtime;
}

[[nodiscard]] UiBindingTable Bindings(std::string address = {}) {
    return {{"address", UiBindingValue{std::move(address)}}};
}

[[nodiscard]] UiInputFrame Activate() {
    UiInputFrame input;
    input.activatePressed = true;
    return input;
}

[[nodiscard]] UiInputFrame Typed(std::vector<UiTextInput> text) {
    UiInputFrame input;
    input.text = std::move(text);
    return input;
}

[[nodiscard]] UiTextInput Insert(std::string text) {
    return {UiTextInputKind::Insert, std::move(text)};
}

[[nodiscard]] UiTextInput Key(const UiTextInputKind kind) {
    return {kind, {}};
}

// Starts editing the focused field, applies text, submits, and returns what
// was submitted (nullopt when nothing was).
[[nodiscard]] std::optional<std::string> EditAndSubmit(UiRuntime& runtime, const UiBindingTable& bindings,
                                                       std::vector<UiTextInput> text) {
    REQUIRE(runtime.Update(Activate(), bindings, kViewport));
    REQUIRE(runtime.InteractionState().editingElement == "address_field");
    REQUIRE(runtime.Update(Typed(std::move(text)), bindings, kViewport));
    auto submitted = runtime.Update(Activate(), bindings, kViewport);
    REQUIRE(submitted);
    for (const UiActionEvent& event : submitted.value()) {
        if (event.action == "submit_address") {
            CHECK(event.sourceElement == "address_field");
            const auto* text = std::get_if<std::string>(&event.payload);
            REQUIRE(text != nullptr);
            return *text;
        }
    }
    return std::nullopt;
}

[[nodiscard]] const UiTextDraw* FieldText(const UiDrawList& list) {
    // The field's text is the first text command (the button's comes later).
    for (const UiDrawCommand& command : list.commands) {
        if (const auto* text = std::get_if<UiTextDraw>(&command)) return text;
    }
    return nullptr;
}

} // namespace

TEST_CASE("UTF-8 editing helpers step by code point and keep one line") {
    const std::string_view text = "a\xE6\x97\xA5" "b"; // a 日 b
    CHECK(UiCodePointCount(text) == 3U);
    CHECK(UiNextCodePoint(text, 0) == 1U);
    CHECK_MESSAGE(UiNextCodePoint(text, 1) == 4U, "a multi-byte code point is one step");
    CHECK(UiPreviousCodePoint(text, 4) == 1U);
    CHECK(UiPreviousCodePoint(text, 0) == 0U);
    CHECK(UiNextCodePoint(text, text.size()) == text.size());
    CHECK_MESSAGE(UiSingleLineText("a\tb\nc\x7F" "d") == "abcd", "control characters are dropped");
    CHECK_MESSAGE(UiSingleLineText("x\xE6\x97" "y") == "xy", "a cut sequence is dropped");
    CHECK(UiSingleLineText("\xE6\x97\xA5") == "\xE6\x97\xA5");
}

TEST_CASE("gyo.ui version 2 reads, validates and writes text_field") {
    auto parsed = UiDocumentCodec::Parse(kTextFieldDocument, "text-field-test");
    REQUIRE(parsed);
    CHECK(parsed.value().version == 2U);
    auto serialized = UiDocumentCodec::Serialize(parsed.value());
    REQUIRE(serialized);
    auto reparsed = UiDocumentCodec::Parse(serialized.value(), "round-trip");
    REQUIRE(reparsed);
    const UiElement& field = reparsed.value().canvases[0].children[0];
    CHECK(reparsed.value().version == 2U);
    CHECK(field.type == UiElementType::TextField);
    CHECK(field.maxLength == 8U);
    CHECK(field.placeholder == "host:port");
    CHECK(field.placeholderColor == "gray");
    CHECK(field.textStyle.verticalAlign == UiVerticalAlign::Center);
    CHECK(reparsed.value().actions[0].payload == UiActionPayloadType::String);
    auto again = UiDocumentCodec::Serialize(reparsed.value());
    REQUIRE(again);
    CHECK_MESSAGE(again.value() == serialized.value(), "the canonical form is stable");
}

TEST_CASE("gyo.ui rejects a text field outside its contract") {
    const auto rejected = [](const std::string& text) {
        auto parsed = UiDocumentCodec::Parse(text, "text-field-test");
        return parsed ? std::optional<UiErrorCode>{} : std::optional<UiErrorCode>{parsed.error().code};
    };
    const std::string document(kTextFieldDocument);
    CHECK_MESSAGE(rejected(Replace(document, "\"version\": 2", "\"version\": 1")) == UiErrorCode::UnsupportedVersion,
                  "version 1 has no text_field and no string payload");
    CHECK_MESSAGE(rejected(Replace(Replace(document, "\"version\": 2", "\"version\": 1"),
                                   "{ \"id\": \"submit_address\", \"payload\": \"string\" }",
                                   "{ \"id\": \"submit_address\", \"payload\": \"none\" }")) ==
                      UiErrorCode::UnsupportedVersion,
                  "a text_field in version 1 is a version error before any type mismatch");
    CHECK(rejected(Replace(document, "\"max_length\": 8", "\"max_length\": 0")) == UiErrorCode::ValidationFailed);
    CHECK(rejected(Replace(document, "\"action\": \"submit_address\"", "\"action\": \"go\"")) ==
          UiErrorCode::BindingTypeMismatch);
    CHECK(rejected(Replace(document, "{ \"id\": \"address\", \"type\": \"string\", \"preview\": \"\" }",
                           "{ \"id\": \"address\", \"type\": \"number\", \"preview\": 0 }")) ==
          UiErrorCode::BindingTypeMismatch);
    CHECK(rejected(Replace(document, "\"placeholder\": \"host:port\",", "")) == UiErrorCode::ValidationFailed);
    CHECK_MESSAGE(rejected(Replace(document, "\"max_length\": 8", "\"max_length\": 8, \"text\": { \"literal\": \"x\" }")) ==
                      UiErrorCode::ValidationFailed,
                  "a text field shows its binding, not a text source");
    CHECK(rejected(Replace(document, "\"focus_order\": [\"address_field\", \"go_button\"]",
                           "\"focus_order\": [\"go_button\"]")) == UiErrorCode::ValidationFailed);
}

TEST_CASE("A text field edits a copy of its binding and submits it as a string action") {
    UiRuntime runtime = Runtime();
    const UiBindingTable bindings = Bindings("10.0");
    CHECK(EditAndSubmit(runtime, bindings, {Insert(".1"), Key(UiTextInputKind::MoveHome), Insert("1")}) == "110.0.1");
    CHECK_MESSAGE(runtime.InteractionState().editingElement.empty(), "a submit ends the edit");

    CHECK(EditAndSubmit(runtime, Bindings("ab"), {Key(UiTextInputKind::Backspace)}) == "a");
    CHECK(EditAndSubmit(runtime, Bindings("ab"), {Key(UiTextInputKind::MoveLeft), Key(UiTextInputKind::MoveLeft),
                                                   Key(UiTextInputKind::Delete)}) == "b");
    CHECK(EditAndSubmit(runtime, Bindings("ab"), {Key(UiTextInputKind::MoveHome), Key(UiTextInputKind::MoveEnd),
                                                   Insert("c")}) == "abc");
    CHECK_MESSAGE(EditAndSubmit(runtime, Bindings("a\xE6\x97\xA5"), {Key(UiTextInputKind::Backspace)}) == "a",
                  "Backspace removes a whole code point");
    CHECK_MESSAGE(EditAndSubmit(runtime, Bindings("ab"), {Key(UiTextInputKind::SelectAll), Insert("z")}) == "z",
                  "typing replaces a selection");
    CHECK(EditAndSubmit(runtime, Bindings("ab"), {Key(UiTextInputKind::SelectAll), Key(UiTextInputKind::Backspace)}) == "");
    CHECK_MESSAGE(EditAndSubmit(runtime, Bindings(), {Insert("123456789")}) == "12345678",
                  "max_length counts code points and stops insertion");
    CHECK(EditAndSubmit(runtime, Bindings(), {Insert("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE6\x97\xA5\xE6\x9C\xAC"
                                                     "\xE8\xAA\x9E\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E")}) ==
          "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE6\x97\xA5\xE6\x9C\xAC");
    CHECK_MESSAGE(EditAndSubmit(runtime, Bindings(), {Insert("a\tb\n")}) == "ab", "a field is one line");
}

// Design Invariant 5 (guard): which characters are kept is the product's
// check function; the same document keeps different text under another one.
TEST_CASE("A text field keeps only what the product's check function returns") {
    UiRuntime runtime = Runtime();
    CHECK_MESSAGE(EditAndSubmit(runtime, Bindings(), {Insert("a1b2")}) == "a1b2", "without a filter all is kept");
    std::vector<std::string> seen;
    runtime.SetTextFilter([&seen](const std::string_view element, const std::string_view text) {
        seen.emplace_back(std::string(element) + ":" + std::string(text));
        std::string digits;
        for (const char c : text) {
            if (c >= '0' && c <= '9') digits += c;
        }
        return digits;
    });
    CHECK(EditAndSubmit(runtime, Bindings(), {Insert("a1b2")}) == "12");
    CHECK(seen == std::vector<std::string>{"address_field:a1b2"});
}

// Design Invariant 6 (guard): a composition is shown, never edited or submitted.
TEST_CASE("A text field shows a composition but never submits it") {
    UiRuntime runtime = Runtime();
    const UiBindingTable bindings = Bindings("ab");
    const UiTextInput composing{UiTextInputKind::Composition, "\xE3\x81\xAB\xE3\x81\xBB", 1};

    CHECK_MESSAGE(EditAndSubmit(runtime, bindings, {Key(UiTextInputKind::MoveLeft), composing}) == "ab",
                  "Enter during a composition submits only the committed text");

    REQUIRE(runtime.Update(Activate(), bindings, kViewport));
    REQUIRE(runtime.Update(Typed({Key(UiTextInputKind::MoveLeft), composing}), bindings, kViewport));
    auto composed = runtime.Compose(bindings, kViewport);
    REQUIRE(composed);
    const UiTextDraw* text = FieldText(composed.value());
    REQUIRE(text != nullptr);
    CHECK_MESSAGE(text->utf8 == "a\xE3\x81\xAB\xE3\x81\xBB" "b", "the composition is shown at the cursor");
    REQUIRE(text->caret.has_value());
    CHECK(text->caret->compositionBegin == 1U);
    CHECK(text->caret->compositionEnd == 7U);
    CHECK_MESSAGE(text->caret->cursor == 4U, "the caret follows the composition's own cursor");

    UiInputFrame cancel;
    cancel.cancelPressed = true;
    auto cancelled = runtime.Update(cancel, bindings, kViewport);
    REQUIRE(cancelled);
    CHECK_MESSAGE(cancelled.value().empty(), "cancel discards the edit and is not the canvas cancel action");
    CHECK(runtime.InteractionState().editingElement.empty());

    REQUIRE(runtime.Update(Activate(), bindings, kViewport));
    REQUIRE(runtime.Update(Typed({composing}), bindings, kViewport));
    UiInputFrame next;
    next.focusNextPressed = true;
    auto moved = runtime.Update(next, bindings, kViewport);
    REQUIRE(moved);
    CHECK_MESSAGE(moved.value().empty(), "losing focus submits nothing");
    CHECK(runtime.InteractionState().editingElement.empty());
    CHECK(runtime.InteractionState().focusedElement == "go_button");
}

// Design Invariant 4: the edit is a copy that a change of the binding overtakes.
TEST_CASE("A text field discards its edit when the binding changes or the pointer leaves") {
    UiRuntime runtime = Runtime();
    REQUIRE(runtime.Update(Activate(), Bindings("old"), kViewport));
    REQUIRE(runtime.Update(Typed({Insert("x")}), Bindings("old"), kViewport));
    auto overtaken = runtime.Update(Activate(), Bindings("new"), kViewport);
    REQUIRE(overtaken);
    CHECK_MESSAGE(overtaken.value().empty(), "an edit of a value that changed is not submitted");
    CHECK(runtime.InteractionState().editingElement.empty());

    UiInputFrame pressField;
    pressField.pointerAvailable = true;
    pressField.pointerPrimaryPressed = true;
    pressField.pointerPixels = {10.0F, 10.0F};
    REQUIRE(runtime.Update(pressField, Bindings(), kViewport));
    CHECK_MESSAGE(runtime.InteractionState().editingElement == "address_field", "a press on the field starts editing");

    UiInputFrame hoverButton;
    hoverButton.pointerAvailable = true;
    hoverButton.pointerPixels = {10.0F, 60.0F};
    REQUIRE(runtime.Update(hoverButton, Bindings(), kViewport));
    hoverButton.pointerPixels = {12.0F, 60.0F};
    REQUIRE(runtime.Update(hoverButton, Bindings(), kViewport));
    CHECK_MESSAGE(runtime.InteractionState().editingElement == "address_field",
                  "moving the pointer over another element does not end the edit");
    CHECK(runtime.InteractionState().focusedElement == "address_field");

    UiInputFrame pressButton = hoverButton;
    pressButton.pointerPrimaryPressed = true;
    auto pressed = runtime.Update(pressButton, Bindings(), kViewport);
    REQUIRE(pressed);
    CHECK(runtime.InteractionState().editingElement.empty());
    REQUIRE(pressed.value().size() == 1U);
    CHECK_MESSAGE(pressed.value()[0].action == "go", "a press elsewhere ends the edit and acts as usual");
}

TEST_CASE("A text field shows its binding, or its placeholder when empty") {
    UiRuntime runtime = Runtime();
    auto empty = runtime.Compose(Bindings(), kViewport);
    REQUIRE(empty);
    const UiTextDraw* placeholder = FieldText(empty.value());
    REQUIRE(placeholder != nullptr);
    CHECK(placeholder->utf8 == "host:port");
    CHECK_MESSAGE(placeholder->color.red == doctest::Approx(placeholder->color.green),
                  "the placeholder uses its own color");
    CHECK(placeholder->color.red < 0.5F);
    CHECK_FALSE(placeholder->caret.has_value());

    auto filled = runtime.Compose(Bindings("10.0.0.1"), kViewport);
    REQUIRE(filled);
    const UiTextDraw* value = FieldText(filled.value());
    REQUIRE(value != nullptr);
    CHECK(value->utf8 == "10.0.0.1");
    CHECK(value->color.red == doctest::Approx(1.0F));
    CHECK_MESSAGE(value->clipPixels.width <= 80.0F, "the text is clipped to the field");

    REQUIRE(runtime.Update(Activate(), Bindings("ab"), kViewport));
    REQUIRE(runtime.Update(Typed({Key(UiTextInputKind::SelectAll)}), Bindings("ab"), kViewport));
    auto editing = runtime.Compose(Bindings("ab"), kViewport);
    REQUIRE(editing);
    const UiTextDraw* edited = FieldText(editing.value());
    REQUIRE(edited != nullptr);
    REQUIRE(edited->caret.has_value());
    CHECK(edited->caret->cursor == 2U);
    CHECK(edited->caret->selectionBegin == 0U);
    CHECK(edited->caret->selectionEnd == 2U);
}

} // namespace Engine::Ui::Tests
