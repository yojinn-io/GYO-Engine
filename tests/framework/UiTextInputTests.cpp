// The shared text input mapping (docs/framework/architecture.md, Design
// Invariant 5; v8-input-ui-adapter): one stream of Engine's text input
// entries gives the Ui text input the game gave before the mapping moved to the
// framework.
#include <doctest/doctest.h>

#include "gyo/framework/UiTextInput.hpp"

#include <functional>
#include <string>
#include <vector>

namespace {

using Engine::Input::Key;
using Engine::Input::TextEvent;
using Engine::Ui::UiTextInput;
using Engine::Ui::UiTextInputKind;
constexpr auto Ctrl = Gyo::Tools::Framework::TextShortcutModifier::Ctrl;
constexpr auto Gui = Gyo::Tools::Framework::TextShortcutModifier::Gui;

// The mapping a game kept as its own until v8-input-ui-adapter (master
// d03c01e2), copied as the reference: the clipboard was the platform's, here it
// is the caller's.
std::vector<UiTextInput> GameReference(const std::vector<TextEvent>& text, const std::function<std::string()>& clipboard) {
    std::vector<UiTextInput> result;
    for (const auto& entry : text) {
        if (entry.kind == TextEvent::Kind::Text) {
            result.push_back({UiTextInputKind::Insert, entry.text});
        } else if (entry.kind == TextEvent::Kind::Composition) {
            result.push_back({UiTextInputKind::Composition, entry.text, entry.cursor});
        } else if (entry.modifiers.ctrl && entry.layoutKey == Key::A) {
            result.push_back({UiTextInputKind::SelectAll});
        } else if (entry.modifiers.ctrl && entry.layoutKey == Key::V) {
            result.push_back({UiTextInputKind::Insert, clipboard()});
        } else if (entry.layoutKey == Key::Backspace) {
            result.push_back({UiTextInputKind::Backspace});
        } else if (entry.layoutKey == Key::Delete) {
            result.push_back({UiTextInputKind::Delete});
        } else if (entry.layoutKey == Key::Left) {
            result.push_back({UiTextInputKind::MoveLeft});
        } else if (entry.layoutKey == Key::Right) {
            result.push_back({UiTextInputKind::MoveRight});
        } else if (entry.layoutKey == Key::Home) {
            result.push_back({UiTextInputKind::MoveHome});
        } else if (entry.layoutKey == Key::End) {
            result.push_back({UiTextInputKind::MoveEnd});
        }
    }
    return result;
}

TextEvent Typed(std::string text) {
    TextEvent event;
    event.kind = TextEvent::Kind::Text;
    event.text = std::move(text);
    return event;
}

TextEvent Composing(std::string text, const std::int32_t cursor) {
    TextEvent event;
    event.kind = TextEvent::Kind::Composition;
    event.text = std::move(text);
    event.cursor = cursor;
    return event;
}

// A key press: physical is the position, layout the key the layout makes there.
TextEvent Pressed(const Key physical, const Key layout, const bool ctrl = false, const bool repeat = false) {
    TextEvent event;
    event.kind = TextEvent::Kind::Key;
    event.key = physical;
    event.layoutKey = layout;
    event.modifiers.ctrl = ctrl;
    event.repeat = repeat;
    return event;
}

TextEvent Pressed(const Key key) { return Pressed(key, key); }

// A key press with Gui (Command on macOS) held.
TextEvent PressedWithGui(const Key key) {
    TextEvent event = Pressed(key, key);
    event.modifiers.gui = true;
    return event;
}

void CheckSame(const std::vector<UiTextInput>& actual, const std::vector<UiTextInput>& expected) {
    REQUIRE(actual.size() == expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        CAPTURE(i);
        CHECK(actual[i].kind == expected[i].kind);
        CHECK(actual[i].text == expected[i].text);
        CHECK(actual[i].cursor == expected[i].cursor);
    }
}

} // namespace

TEST_CASE("U1: a stream of every kind of entry maps as the game mapped it") {
    const std::vector<TextEvent> events{
        Typed("a"),
        Typed("\xE3\x81\x82"),
        Composing("\xE3\x81\xAB\xE3\x81\xBB", 0),
        Composing("\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93", -1),
        Composing("\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93\xE3\x81\x94", 2),
        Composing("", -1),
        Typed("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"),
        Pressed(Key::A, Key::A, true),
        Pressed(Key::V, Key::V, true),
        Pressed(Key::Backspace),
        Pressed(Key::Backspace, Key::Backspace, false, true),
        Pressed(Key::Delete),
        Pressed(Key::Left),
        Pressed(Key::Right, Key::Right, false, true),
        Pressed(Key::Home),
        Pressed(Key::End),
        Pressed(Key::F1),
        Pressed(Key::Enter),
        Pressed(Key::Escape),
        // AZERTY: A is made on Q's position, and Q on A's (also checked one by one in U5).
        Pressed(Key::Q, Key::A, true),
        Typed("q"),
        Pressed(Key::A, Key::Q, true),
        // Without Ctrl, A and V are only keys; their text comes as Text.
        Pressed(Key::A),
        Pressed(Key::V),
    };
    const std::function<std::string()> clipboard = [] { return std::string{"pasted"}; };
    const auto actual = Gyo::Tools::Framework::ToUiTextInput(events, Ctrl, clipboard);
    CheckSame(actual, GameReference(events, clipboard));

    const std::vector<UiTextInput> expected{
        {UiTextInputKind::Insert, "a"},
        {UiTextInputKind::Insert, "\xE3\x81\x82"},
        {UiTextInputKind::Composition, "\xE3\x81\xAB\xE3\x81\xBB", 0},
        {UiTextInputKind::Composition, "\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93", -1},
        {UiTextInputKind::Composition, "\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93\xE3\x81\x94", 2},
        {UiTextInputKind::Composition, "", -1},
        {UiTextInputKind::Insert, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"},
        {UiTextInputKind::SelectAll},
        {UiTextInputKind::Insert, "pasted"},
        {UiTextInputKind::Backspace},
        {UiTextInputKind::Backspace},
        {UiTextInputKind::Delete},
        {UiTextInputKind::MoveLeft},
        {UiTextInputKind::MoveRight},
        {UiTextInputKind::MoveHome},
        {UiTextInputKind::MoveEnd},
        {UiTextInputKind::SelectAll},
        {UiTextInputKind::Insert, "q"},
    };
    CheckSame(actual, expected);
}

TEST_CASE("U2: Ctrl+V inserts what the clipboard has, an empty clipboard included") {
    const std::vector<TextEvent> paste{Pressed(Key::V, Key::V, true)};
    int calls = 0;
    const auto empty = Gyo::Tools::Framework::ToUiTextInput(paste, Ctrl, [&] {
        ++calls;
        return std::string{};
    });
    CHECK(calls == 1);
    CheckSame(empty, {{UiTextInputKind::Insert, ""}});
}

TEST_CASE("U3: the clipboard is read only for Ctrl+V") {
    const std::vector<TextEvent> events{Typed("x"), Pressed(Key::A, Key::A, true), Pressed(Key::V)};
    int calls = 0;
    const auto actual = Gyo::Tools::Framework::ToUiTextInput(events, Ctrl, [&] {
        ++calls;
        return std::string{"never"};
    });
    CHECK(calls == 0);
    CheckSame(actual, {{UiTextInputKind::Insert, "x"}, {UiTextInputKind::SelectAll}});
}

TEST_CASE("U4: no entries give no text input") {
    CHECK(Gyo::Tools::Framework::ToUiTextInput({}, Ctrl, [] { return std::string{}; }).empty());
}

TEST_CASE("U5: Ctrl+A is the key the layout produces, not the position") {
    const auto clipboard = [] { return std::string{}; };
    CheckSame(Gyo::Tools::Framework::ToUiTextInput(std::vector<TextEvent>{Pressed(Key::Q, Key::A, true)}, Ctrl, clipboard),
              {{UiTextInputKind::SelectAll}});
    CHECK(Gyo::Tools::Framework::ToUiTextInput(std::vector<TextEvent>{Pressed(Key::A, Key::Q, true)}, Ctrl, clipboard).empty());
}

TEST_CASE("U6: the shortcuts take the modifier the product chose, and only it") {
    const std::vector<TextEvent> events{PressedWithGui(Key::A), PressedWithGui(Key::V), Pressed(Key::A, Key::A, true),
                                        Pressed(Key::V, Key::V, true)};
    const auto clipboard = [] { return std::string{"pasted"}; };
    CheckSame(Gyo::Tools::Framework::ToUiTextInput(events, Gui, clipboard),
              {{UiTextInputKind::SelectAll}, {UiTextInputKind::Insert, "pasted"}});
    CheckSame(Gyo::Tools::Framework::ToUiTextInput(events, Ctrl, clipboard),
              {{UiTextInputKind::SelectAll}, {UiTextInputKind::Insert, "pasted"}});
    // With Gui chosen, Ctrl+A and Ctrl+V are only keys, and the other way round.
    CHECK(Gyo::Tools::Framework::ToUiTextInput(std::vector<TextEvent>{Pressed(Key::A, Key::A, true)}, Gui, clipboard).empty());
    CHECK(Gyo::Tools::Framework::ToUiTextInput(std::vector<TextEvent>{PressedWithGui(Key::V)}, Ctrl, clipboard).empty());
}
