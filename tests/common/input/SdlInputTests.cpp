#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/input/backend/sdl/SdlInput.hpp"
#include "engine/input/backend/sdl/SdlInputNative.hpp"
#include "engine/input/InputActionMap.hpp"

#include <array>
#include <chrono>
#include <utility>

using namespace Engine::Input;

TEST_CASE("SDL space H and F3 events retain press hold release and focus semantics") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    const std::array bindings{
        std::pair{SDL_SCANCODE_SPACE, Key::Space},
        std::pair{SDL_SCANCODE_H, Key::H},
        std::pair{SDL_SCANCODE_F3, Key::F3},
    };

    for (const auto& [scancode, key] : bindings) {
        InputActionMap actions;
        const auto action = InputActionId::FromString("test_action");
        actions.Bind(action, key);
        input.BeginFrame();
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = window + 1;
        event.key.scancode = scancode;
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, event, Engine::Time::TimePoint{});
        CHECK_FALSE(input.Snapshot().Get(key).held);

        event.key.windowID = window;
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, event, Engine::Time::TimePoint{});
        auto state = actions.Evaluate(input.Snapshot()).Action(action);
        CHECK(state.held);
        CHECK(state.pressed);
        CHECK_FALSE(state.released);

        input.BeginFrame();
        event.key.repeat = true;
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, event, Engine::Time::TimePoint{});
        state = actions.Evaluate(input.Snapshot()).Action(action);
        CHECK(state.held);
        CHECK_FALSE(state.pressed);
        CHECK_FALSE(state.released);

        event.key.repeat = false;
        event.type = SDL_EVENT_KEY_UP;
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, event, Engine::Time::TimePoint{});
        state = actions.Evaluate(input.Snapshot()).Action(action);
        CHECK_FALSE(state.held);
        CHECK_FALSE(state.pressed);
        CHECK(state.released);
        input.BeginFrame();
        CHECK_FALSE(input.Snapshot().Get(key).released);
    }

    // Focus loss releases keys so interrupted jump/holster/debug presses
    // cannot remain held after returning to an application.
    input.BeginFrame();
    for (const auto& [scancode, key] : bindings) {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = window;
        event.key.scancode = scancode;
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, event, Engine::Time::TimePoint{});
        REQUIRE(input.Snapshot().Get(key).held);
    }
    input.BeginFrame();
    SDL_Event lost{};
    lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    lost.window.windowID = window;
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, lost, Engine::Time::TimePoint{});
    CHECK_FALSE(input.Snapshot().windowFocused);
    for (const auto& [scancode, key] : bindings) {
        static_cast<void>(scancode);
        CHECK_FALSE(input.Snapshot().Get(key).held);
        CHECK(input.Snapshot().Get(key).released);
    }
}

namespace {
SDL_Event KeyEvent(SDL_WindowID window, SDL_Scancode scancode, bool down, bool repeat = false) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = window;
    event.key.scancode = scancode;
    event.key.repeat = repeat;
    return event;
}
SDL_Event ButtonEvent(SDL_WindowID window, Uint8 button, bool down, float x, float y) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = window;
    event.button.button = button;
    event.button.x = x;
    event.button.y = y;
    return event;
}
SDL_Event WindowEvent(SDL_WindowID window, SDL_EventType type, Sint32 data1 = 0, Sint32 data2 = 0) {
    SDL_Event event{};
    event.type = type;
    event.window.windowID = window;
    event.window.data1 = data1;
    event.window.data2 = data2;
    return event;
}
} // namespace

// The first fourteen keys keep their original values, so stored or hashed
// values and existing bindings stay valid after the keyboard was completed.
static_assert(static_cast<std::size_t>(Key::W) == 0 && static_cast<std::size_t>(Key::A) == 1 &&
              static_cast<std::size_t>(Key::S) == 2 && static_cast<std::size_t>(Key::D) == 3 &&
              static_cast<std::size_t>(Key::Left) == 4 && static_cast<std::size_t>(Key::Right) == 5 &&
              static_cast<std::size_t>(Key::Up) == 6 && static_cast<std::size_t>(Key::Down) == 7 &&
              static_cast<std::size_t>(Key::R) == 8 && static_cast<std::size_t>(Key::Enter) == 9 &&
              static_cast<std::size_t>(Key::Escape) == 10 && static_cast<std::size_t>(Key::Space) == 11 &&
              static_cast<std::size_t>(Key::H) == 12 && static_cast<std::size_t>(Key::F3) == 13);
static_assert(static_cast<std::size_t>(MouseButton::Left) == 0 && static_cast<std::size_t>(MouseButton::Right) == 1 &&
              static_cast<std::size_t>(MouseButton::Middle) == 2);

TEST_CASE("every engine key is reachable from an SDL scancode and the original keys map as before") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    std::array<int, static_cast<std::size_t>(Key::Count)> sources{};
    for (int code = 0; code < SDL_SCANCODE_COUNT; ++code) {
        input.BeginFrame();
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(window, static_cast<SDL_Scancode>(code), true), Engine::Time::TimePoint{});
        const auto& events = input.Snapshot().events;
        if (events.empty()) continue;
        REQUIRE(events.size() == 1);
        CHECK(events[0].kind == InputEventKind::KeyPressed);
        REQUIRE(events[0].key != Key::Count);
        ++sources[static_cast<std::size_t>(events[0].key)];
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(window, static_cast<SDL_Scancode>(code), false), Engine::Time::TimePoint{});
    }
    for (std::size_t key = 0; key < sources.size(); ++key) {
        CAPTURE(key);
        CHECK(sources[key] == (key == static_cast<std::size_t>(Key::Enter) ? 2 : 1));
    }
    const std::array original{
        std::pair{SDL_SCANCODE_W, Key::W}, std::pair{SDL_SCANCODE_A, Key::A}, std::pair{SDL_SCANCODE_S, Key::S},
        std::pair{SDL_SCANCODE_D, Key::D}, std::pair{SDL_SCANCODE_LEFT, Key::Left},
        std::pair{SDL_SCANCODE_RIGHT, Key::Right}, std::pair{SDL_SCANCODE_UP, Key::Up},
        std::pair{SDL_SCANCODE_DOWN, Key::Down}, std::pair{SDL_SCANCODE_R, Key::R},
        std::pair{SDL_SCANCODE_RETURN, Key::Enter}, std::pair{SDL_SCANCODE_KP_ENTER, Key::Enter},
        std::pair{SDL_SCANCODE_ESCAPE, Key::Escape}, std::pair{SDL_SCANCODE_SPACE, Key::Space},
        std::pair{SDL_SCANCODE_H, Key::H}, std::pair{SDL_SCANCODE_F3, Key::F3},
        std::pair{SDL_SCANCODE_TAB, Key::Tab}, std::pair{SDL_SCANCODE_1, Key::Digit1},
        std::pair{SDL_SCANCODE_6, Key::Digit6},
    };
    for (const auto& [scancode, key] : original) {
        input.BeginFrame();
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(window, scancode, true), Engine::Time::TimePoint{});
        CHECK(input.Snapshot().Get(key).held);
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(window, scancode, false), Engine::Time::TimePoint{});
    }
}

TEST_CASE("a press and release in one frame keep both transitions in order with the click position") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    int width{}, height{};
    REQUIRE(SDL_GetWindowSize(Engine::Platform::Sdl::NativeWindow(*platform.value()), &width, &height));
    REQUIRE(width > 2);
    REQUIRE(height > 2);

    input.BeginFrame();
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(window, SDL_BUTTON_LEFT, true, 1.0f, 2.0f), Engine::Time::TimePoint{});
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(window, SDL_BUTTON_LEFT, false, 1.5f, 2.5f), Engine::Time::TimePoint{});
    const auto& frame = input.Snapshot();
    // Existing semantics are unchanged: the last transition wins.
    CHECK_FALSE(frame.Get(MouseButton::Left).held);
    CHECK_FALSE(frame.Get(MouseButton::Left).pressed);
    CHECK(frame.Get(MouseButton::Left).released);
    REQUIRE(frame.events.size() == 2);
    CHECK(frame.events[0].kind == InputEventKind::MouseButtonPressed);
    CHECK(frame.events[0].button == MouseButton::Left);
    CHECK(frame.events[0].x == 1.0f);
    CHECK(frame.events[0].y == 2.0f);
    CHECK(frame.events[0].insideWindow);
    CHECK(frame.events[1].kind == InputEventKind::MouseButtonReleased);
    CHECK(frame.events[1].x == 1.5f);

    input.BeginFrame();
    CHECK(input.Snapshot().events.empty());
    for (const auto& [x, y] : {std::pair{-1.0f, 2.0f}, std::pair{2.0f, -0.5f},
                               std::pair{static_cast<float>(width), 2.0f}, std::pair{2.0f, static_cast<float>(height)}}) {
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(window, SDL_BUTTON_RIGHT, true, x, y), Engine::Time::TimePoint{});
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(window, SDL_BUTTON_RIGHT, false, x, y), Engine::Time::TimePoint{});
    }
    REQUIRE(input.Snapshot().events.size() == 8);
    for (const auto& event : input.Snapshot().events) CHECK_FALSE(event.insideWindow);

    // Keys alike, and repeats are not events.
    input.BeginFrame();
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(window, SDL_SCANCODE_TAB, true), Engine::Time::TimePoint{});
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(window, SDL_SCANCODE_TAB, true, true), Engine::Time::TimePoint{});
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(window, SDL_SCANCODE_TAB, false), Engine::Time::TimePoint{});
    REQUIRE(input.Snapshot().events.size() == 2);
    CHECK(input.Snapshot().events[0].kind == InputEventKind::KeyPressed);
    CHECK(input.Snapshot().events[0].key == Key::Tab);
    CHECK(input.Snapshot().events[1].kind == InputEventKind::KeyReleased);
    CHECK(input.Snapshot().Get(Key::Tab).released);
}

TEST_CASE("focus events of this window set and clear windowFocused") {
    // The product's pointer capture reads windowFocused as focusedAtStart (sdl-isolation J1).
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    input.BeginFrame();
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(window, SDL_EVENT_WINDOW_FOCUS_LOST), Engine::Time::TimePoint{});
    CHECK_FALSE(input.Snapshot().windowFocused);
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(window, SDL_EVENT_WINDOW_FOCUS_GAINED), Engine::Time::TimePoint{});
    CHECK_MESSAGE(input.Snapshot().windowFocused, "a focus gain sets windowFocused");
    input.BeginFrame();
    CHECK_MESSAGE(input.Snapshot().windowFocused, "focus carries over to the next frame");
}

TEST_CASE("events of this window are not deduplicated when the state does not change") {
    // input.md (Contracts): every focus loss, key press and mouse button event SDL reports for this window is an
    // event even when the state does not change; only key repeats are dropped. The product's pointer capture
    // relies on it (sdl-isolation J1).
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    const auto count = [&](InputEventKind kind) {
        std::size_t n = 0;
        for (const auto& event : input.Snapshot().events) n += event.kind == kind;
        return n;
    };
    input.BeginFrame();
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(window, SDL_EVENT_WINDOW_FOCUS_LOST), Engine::Time::TimePoint{});
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(window, SDL_EVENT_WINDOW_FOCUS_LOST), Engine::Time::TimePoint{});
    CHECK_MESSAGE(count(InputEventKind::WindowFocusLost) == 2, "a focus loss while unfocused is still an event");
    input.BeginFrame();
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(window, SDL_SCANCODE_TAB, true), Engine::Time::TimePoint{});
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(window, SDL_SCANCODE_TAB, true), Engine::Time::TimePoint{});
    CHECK_MESSAGE(count(InputEventKind::KeyPressed) == 2, "a second non-repeat key press is still an event");
    input.BeginFrame();
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(window, SDL_BUTTON_LEFT, true, 1.0f, 2.0f), Engine::Time::TimePoint{});
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(window, SDL_BUTTON_LEFT, true, 1.0f, 2.0f), Engine::Time::TimePoint{});
    CHECK_MESSAGE(count(InputEventKind::MouseButtonPressed) == 2, "a second mouse button press is still an event");
}

TEST_CASE("a click is inside the window by the window size when the backend handles it") {
    // input.md (Contracts): insideWindow uses the size at handling, never an
    // earlier one; the product's pointer capture relies on it (sdl-isolation J1).
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    const auto before = platform.value()->QueryWindow().rect;
    REQUIRE(before.width > 40);
    const float x = static_cast<float>(before.width) - 10.0f;  // inside the first size only

    input.BeginFrame();
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(window, SDL_BUTTON_LEFT, true, x, 2.0f), Engine::Time::TimePoint{});
    REQUIRE(input.Snapshot().events.size() == 1);
    CHECK(input.Snapshot().events[0].insideWindow);

    REQUIRE(platform.value()->SetWindowSize(before.width / 2, before.height / 2));
    const auto after = platform.value()->QueryWindow().rect;
    REQUIRE(static_cast<float>(after.width) <= x);
    input.BeginFrame();
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(window, SDL_BUTTON_LEFT, false, x, 2.0f), Engine::Time::TimePoint{});
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(window, SDL_BUTTON_LEFT, true, 1.0f, 2.0f), Engine::Time::TimePoint{});
    REQUIRE(input.Snapshot().events.size() == 2);
    CHECK_MESSAGE(!input.Snapshot().events[0].insideWindow, "a click outside the current size is outside");
    CHECK(input.Snapshot().events[1].insideWindow);
}

TEST_CASE("window interaction events of this window are reported in order with their data") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));

    input.BeginFrame();
    for (const SDL_WindowID id : {window + 1, window}) {
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(id, SDL_EVENT_WINDOW_MOVED, 40, 50), Engine::Time::TimePoint{});
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, KeyEvent(id, SDL_SCANCODE_TAB, true), Engine::Time::TimePoint{});
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(id, SDL_EVENT_WINDOW_RESIZED, 800, 600), Engine::Time::TimePoint{});
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, ButtonEvent(id, SDL_BUTTON_LEFT, true, 3.0f, 4.0f), Engine::Time::TimePoint{});
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(id, SDL_EVENT_WINDOW_MINIMIZED), Engine::Time::TimePoint{});
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(id, SDL_EVENT_WINDOW_RESTORED), Engine::Time::TimePoint{});
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(id, SDL_EVENT_WINDOW_FOCUS_LOST), Engine::Time::TimePoint{});
        Engine::Input::Backend::Sdl::HandleNativeEvent(input, WindowEvent(id, SDL_EVENT_WINDOW_FOCUS_GAINED), Engine::Time::TimePoint{});
    }
    // Only the second pass (this window) is reported.
    const auto& events = input.Snapshot().events;
    REQUIRE(events.size() == 8);
    CHECK(events[0].kind == InputEventKind::WindowMoved);
    CHECK(events[0].windowX == 40);
    CHECK(events[0].windowY == 50);
    CHECK(events[1].kind == InputEventKind::KeyPressed);
    CHECK(events[2].kind == InputEventKind::WindowResized);
    CHECK(events[2].windowWidth == 800);
    CHECK(events[2].windowHeight == 600);
    CHECK(events[3].kind == InputEventKind::MouseButtonPressed);
    CHECK(events[4].kind == InputEventKind::WindowMinimized);
    CHECK(events[5].kind == InputEventKind::WindowRestored);
    CHECK(events[6].kind == InputEventKind::WindowFocusLost);
    CHECK(events[7].kind == InputEventKind::WindowFocusGained);
    // The focus loss released what the second pass held; that release is a
    // state change, not an event.
    CHECK_FALSE(input.Snapshot().Get(Key::Tab).held);
    CHECK_FALSE(input.Snapshot().Get(MouseButton::Left).held);
}

TEST_CASE("SDL input events carry the time of the platform event that added them") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    const Engine::Time::TimePoint pressedAt = Engine::Time::TimePoint{} + std::chrono::seconds(7);
    const Engine::Time::TimePoint focusAt = pressedAt + std::chrono::milliseconds(3);

    input.BeginFrame();
    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.windowID = window;
    key.key.scancode = SDL_SCANCODE_W;
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, key, pressedAt);
    SDL_Event focus{};
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    focus.window.windowID = window;
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, focus, focusAt);

    const auto& events = input.Snapshot().events;
    REQUIRE(events.size() == 2);
    CHECK_MESSAGE(events[0].timestamp == pressedAt, "an input event carries its platform event's time");
    CHECK_MESSAGE(events[1].timestamp == focusAt, "an input event carries its platform event's time");
}

namespace {

SDL_Event TextKey(const SDL_WindowID window, const SDL_Scancode scancode, const SDL_Keycode keycode,
                  const bool down, const bool repeat = false, const SDL_Keymod mod = SDL_KMOD_NONE) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = window;
    event.key.scancode = scancode;
    event.key.key = keycode;
    event.key.mod = mod;
    event.key.down = down;
    event.key.repeat = repeat;
    return event;
}

SDL_Event TypedText(const SDL_WindowID window, const char* text) {
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = window;
    event.text.text = text;
    return event;
}

} // namespace

TEST_CASE("SDL text input collects text only while it is on") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    const Engine::Time::TimePoint at{};

    input.BeginFrame();
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, TypedText(window, "x"), at);
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, TextKey(window, SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE, true), at);
    CHECK_MESSAGE(input.Snapshot().text.empty(), "nothing is collected while text input is off");

    REQUIRE(input.SetTextInput(true));
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, TypedText(window, "ab"), at);
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, TypedText(window + 1, "other window"), at);
    REQUIRE(input.Snapshot().text.size() == 1);
    CHECK(input.Snapshot().text[0].kind == TextEvent::Kind::Text);
    CHECK(input.Snapshot().text[0].text == "ab");

    input.BeginFrame();
    CHECK_MESSAGE(input.Snapshot().text.empty(), "BeginFrame clears the text");

    REQUIRE(input.SetTextInput(false));
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, TypedText(window, "y"), at);
    CHECK_MESSAGE(input.Snapshot().text.empty(), "nothing is collected after text input is turned off");
}

TEST_CASE("SDL text keys record presses and repeats in arrival order but never releases") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    const Engine::Time::TimePoint at{};
    REQUIRE(input.SetTextInput(true));

    input.BeginFrame();
    using Engine::Input::Backend::Sdl::HandleNativeEvent;
    HandleNativeEvent(input, TypedText(window, "1"), at);
    HandleNativeEvent(input, TextKey(window, SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE, true), at);
    HandleNativeEvent(input, TextKey(window, SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE, true, true), at);
    HandleNativeEvent(input, TextKey(window, SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE, false), at);
    HandleNativeEvent(input, TypedText(window, "2"), at);
    HandleNativeEvent(input, TextKey(window, SDL_SCANCODE_V, SDLK_V, true, false, SDL_KMOD_LCTRL), at);

    const auto& text = input.Snapshot().text;
    REQUIRE_MESSAGE(text.size() == 5, "a release is never recorded as a text key");
    CHECK_MESSAGE((text[0].kind == TextEvent::Kind::Text && text[0].text == "1"), "text keeps its arrival order");
    CHECK_MESSAGE((text[1].kind == TextEvent::Kind::Key && text[1].layoutKey == Key::Backspace && !text[1].repeat),
                  "text keeps its arrival order");
    CHECK_MESSAGE((text[2].kind == TextEvent::Kind::Key && text[2].repeat), "a key repeat is recorded as a text key");
    CHECK_MESSAGE((text[3].kind == TextEvent::Kind::Text && text[3].text == "2"), "text keeps its arrival order");
    CHECK_MESSAGE((text[4].layoutKey == Key::V && text[4].modifiers.ctrl && !text[4].modifiers.shift),
                  "a text key carries its modifiers");

    // The repeat is not an event (IP-1): one press and one release only.
    const auto& events = input.Snapshot().events;
    std::size_t backspaceEvents = 0;
    for (const auto& event : events)
        if (event.key == Key::Backspace) ++backspaceEvents;
    CHECK_MESSAGE(backspaceEvents == 2, "a key repeat is not an input event");
}

TEST_CASE("SDL text keys carry the layout key apart from the physical key") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform.value()));
    REQUIRE(input.SetTextInput(true));

    input.BeginFrame();
    // AZERTY: the Q position produces 'a'.
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, TextKey(window, SDL_SCANCODE_Q, SDLK_A, true),
                                                   Engine::Time::TimePoint{});
    // A key without a character carries its scancode in the keycode.
    Engine::Input::Backend::Sdl::HandleNativeEvent(input, TextKey(window, SDL_SCANCODE_KP_ENTER, SDLK_KP_ENTER, true),
                                                   Engine::Time::TimePoint{});
    const auto& text = input.Snapshot().text;
    REQUIRE(text.size() == 2);
    CHECK_MESSAGE((text[0].key == Key::Q && text[0].layoutKey == Key::A),
                  "the layout key is the key the layout produces, apart from the position");
    CHECK_MESSAGE(text[1].layoutKey == Key::Enter, "a keycode without a character maps through its scancode");
}

TEST_CASE("SdlPlatform reads the clipboard text") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    REQUIRE(SDL_SetClipboardText("127.0.0.1:7777"));
    CHECK_MESSAGE(platform.value()->ClipboardText() == "127.0.0.1:7777", "the clipboard text is read back");
    REQUIRE(SDL_ClearClipboardData());
    CHECK(platform.value()->ClipboardText().empty());
}
