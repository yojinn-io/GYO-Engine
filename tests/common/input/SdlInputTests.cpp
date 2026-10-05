#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/input/backend/sdl/SdlInput.hpp"
#include "engine/input/InputActionMap.hpp"

#include <array>
#include <utility>

using namespace Engine::Input;

TEST_CASE("SDL space H and F3 events retain press hold release and focus semantics") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(platform.value()->NativeWindow());
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
        input.HandleEvent(event);
        CHECK_FALSE(input.Snapshot().Get(key).held);

        event.key.windowID = window;
        input.HandleEvent(event);
        auto state = actions.Evaluate(input.Snapshot()).Action(action);
        CHECK(state.held);
        CHECK(state.pressed);
        CHECK_FALSE(state.released);

        input.BeginFrame();
        event.key.repeat = true;
        input.HandleEvent(event);
        state = actions.Evaluate(input.Snapshot()).Action(action);
        CHECK(state.held);
        CHECK_FALSE(state.pressed);
        CHECK_FALSE(state.released);

        event.key.repeat = false;
        event.type = SDL_EVENT_KEY_UP;
        input.HandleEvent(event);
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
        input.HandleEvent(event);
        REQUIRE(input.Snapshot().Get(key).held);
    }
    input.BeginFrame();
    SDL_Event lost{};
    lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    lost.window.windowID = window;
    input.HandleEvent(lost);
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
    const SDL_WindowID window = SDL_GetWindowID(platform.value()->NativeWindow());
    std::array<int, static_cast<std::size_t>(Key::Count)> sources{};
    for (int code = 0; code < SDL_SCANCODE_COUNT; ++code) {
        input.BeginFrame();
        input.HandleEvent(KeyEvent(window, static_cast<SDL_Scancode>(code), true));
        const auto& events = input.Snapshot().events;
        if (events.empty()) continue;
        REQUIRE(events.size() == 1);
        CHECK(events[0].kind == InputEventKind::KeyPressed);
        REQUIRE(events[0].key != Key::Count);
        ++sources[static_cast<std::size_t>(events[0].key)];
        input.HandleEvent(KeyEvent(window, static_cast<SDL_Scancode>(code), false));
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
        input.HandleEvent(KeyEvent(window, scancode, true));
        CHECK(input.Snapshot().Get(key).held);
        input.HandleEvent(KeyEvent(window, scancode, false));
    }
}

TEST_CASE("a press and release in one frame keep both transitions in order with the click position") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(platform.value()->NativeWindow());
    int width{}, height{};
    REQUIRE(SDL_GetWindowSize(platform.value()->NativeWindow(), &width, &height));
    REQUIRE(width > 2);
    REQUIRE(height > 2);

    input.BeginFrame();
    input.HandleEvent(ButtonEvent(window, SDL_BUTTON_LEFT, true, 1.0f, 2.0f));
    input.HandleEvent(ButtonEvent(window, SDL_BUTTON_LEFT, false, 1.5f, 2.5f));
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
        input.HandleEvent(ButtonEvent(window, SDL_BUTTON_RIGHT, true, x, y));
        input.HandleEvent(ButtonEvent(window, SDL_BUTTON_RIGHT, false, x, y));
    }
    REQUIRE(input.Snapshot().events.size() == 8);
    for (const auto& event : input.Snapshot().events) CHECK_FALSE(event.insideWindow);

    // Keys alike, and repeats are not events.
    input.BeginFrame();
    input.HandleEvent(KeyEvent(window, SDL_SCANCODE_TAB, true));
    input.HandleEvent(KeyEvent(window, SDL_SCANCODE_TAB, true, true));
    input.HandleEvent(KeyEvent(window, SDL_SCANCODE_TAB, false));
    REQUIRE(input.Snapshot().events.size() == 2);
    CHECK(input.Snapshot().events[0].kind == InputEventKind::KeyPressed);
    CHECK(input.Snapshot().events[0].key == Key::Tab);
    CHECK(input.Snapshot().events[1].kind == InputEventKind::KeyReleased);
    CHECK(input.Snapshot().Get(Key::Tab).released);
}

TEST_CASE("window interaction events of this window are reported in order with their data") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(platform.value()->NativeWindow());

    input.BeginFrame();
    for (const SDL_WindowID id : {window + 1, window}) {
        input.HandleEvent(WindowEvent(id, SDL_EVENT_WINDOW_MOVED, 40, 50));
        input.HandleEvent(KeyEvent(id, SDL_SCANCODE_TAB, true));
        input.HandleEvent(WindowEvent(id, SDL_EVENT_WINDOW_RESIZED, 800, 600));
        input.HandleEvent(ButtonEvent(id, SDL_BUTTON_LEFT, true, 3.0f, 4.0f));
        input.HandleEvent(WindowEvent(id, SDL_EVENT_WINDOW_MINIMIZED));
        input.HandleEvent(WindowEvent(id, SDL_EVENT_WINDOW_RESTORED));
        input.HandleEvent(WindowEvent(id, SDL_EVENT_WINDOW_FOCUS_LOST));
        input.HandleEvent(WindowEvent(id, SDL_EVENT_WINDOW_FOCUS_GAINED));
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
