#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "input/backend/sdl/SdlInput.hpp"
#include "engine/input/InputActionMap.hpp"

#include <array>
#include <utility>

using namespace Engine::Input;

TEST_CASE("SDL space and H events retain press hold release and focus semantics") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(platform.value()->NativeWindow());
    const std::array bindings{
        std::pair{SDL_SCANCODE_SPACE, Key::Space},
        std::pair{SDL_SCANCODE_H, Key::H},
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

    // Focus loss releases both keys so an interrupted jump/holster press
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
