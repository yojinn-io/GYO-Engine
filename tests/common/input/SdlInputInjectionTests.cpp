// IP-4 ip4a: injected test input (ip4-declaration.md section 1).
#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/input/backend/sdl/SdlInputInjection.hpp"
#include "engine/input/backend/sdl/SdlInputNative.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <vector>

using namespace Engine::Input;
using Engine::Input::Backend::Sdl::SdlInput;
using Engine::Input::Backend::Sdl::SdlInputInjector;
using Engine::Platform::Sdl::PlatformEvent;
using Engine::Platform::Sdl::SdlPlatform;

namespace {

std::unique_ptr<SdlPlatform> MakeInjectionPlatform() {
    auto platform = SdlPlatform::Create();
    REQUIRE(platform);
    return std::move(platform.value());
}

// One frame: BeginFrame, pump into the input, EndFrame.
void PumpFrame(SdlPlatform& platform, SdlInput& input) {
    input.BeginFrame();
    (void)platform.PumpEvents([&](const PlatformEvent& event) { input.HandleEvent(event); });
    input.EndFrame();
}

// Native events SDL hands to watchers, as a test-side observer of the queue.
struct NativeSeen final {
    std::vector<SDL_Event> events;
};

bool SDLCALL RecordNative(void* userdata, SDL_Event* event) {
    static_cast<NativeSeen*>(userdata)->events.push_back(*event);
    return true;
}

} // namespace

TEST_CASE("Injected input goes through SDL's queue only") {
    auto platform = MakeInjectionPlatform();
    SdlInput input(*platform);
    SdlInputInjector injector(*platform);
    PumpFrame(*platform, input);
    NativeSeen seen;
    REQUIRE(SDL_AddEventWatch(&RecordNative, &seen));
    input.BeginFrame();
    REQUIRE(injector.PushKey(Key::W, true));
    CHECK_MESSAGE(input.Snapshot().events.empty(), "injection does not write the input frame directly");
    CHECK_MESSAGE(seen.events.size() == 1, "each injection pushes exactly one SDL event");
    SDL_RemoveEventWatch(&RecordNative, &seen);
    (void)platform->PumpEvents([&](const PlatformEvent& event) { input.HandleEvent(event); });
    input.EndFrame();
    REQUIRE(input.Snapshot().events.size() == 1);
    CHECK(input.Snapshot().events[0].kind == InputEventKind::KeyPressed);
}

TEST_CASE("Every injected key reaches the next frame as that key") {
    auto platform = MakeInjectionPlatform();
    SdlInput input(*platform);
    SdlInputInjector injector(*platform);
    PumpFrame(*platform, input);  // the window's own start-up events
    for (std::size_t index = 0; index < static_cast<std::size_t>(Key::Count); ++index) {
        const Key key = static_cast<Key>(index);
        REQUIRE(injector.PushKey(key, true));
        PumpFrame(*platform, input);
        const auto& events = input.Snapshot().events;
        REQUIRE_MESSAGE(events.size() == 1, "an injected key reaches the next frame");
        CHECK_MESSAGE((events[0].kind == InputEventKind::KeyPressed && events[0].key == key),
                      "an injected key reaches the next frame as that key");
        REQUIRE(injector.PushKey(key, false));
        PumpFrame(*platform, input);
    }
}

TEST_CASE("An injected Enter carries the Return scancode") {
    auto platform = MakeInjectionPlatform();
    SdlInputInjector injector(*platform);
    NativeSeen seen;
    REQUIRE(SDL_AddEventWatch(&RecordNative, &seen));
    REQUIRE(injector.PushKey(Key::Enter, true));
    SDL_RemoveEventWatch(&RecordNative, &seen);
    REQUIRE(seen.events.size() == 1);
    CHECK_MESSAGE(seen.events[0].key.scancode == SDL_SCANCODE_RETURN, "Enter is injected as Return");
    (void)platform->PumpEvents();
}

TEST_CASE("Injected mouse buttons and focus keep their direction") {
    auto platform = MakeInjectionPlatform();
    SdlInput input(*platform);
    SdlInputInjector injector(*platform);
    PumpFrame(*platform, input);  // the window's own start-up events
    REQUIRE(injector.PushMouseButton(MouseButton::Right, true));
    REQUIRE(injector.PushMouseButton(MouseButton::Right, false));
    REQUIRE(injector.PushFocus(false));
    REQUIRE(injector.PushFocus(true));
    PumpFrame(*platform, input);
    const auto& events = input.Snapshot().events;
    REQUIRE(events.size() == 4);
    CHECK_MESSAGE((events[0].kind == InputEventKind::MouseButtonPressed && events[0].button == MouseButton::Right),
                  "an injected mouse button keeps its button and direction");
    CHECK_MESSAGE((events[1].kind == InputEventKind::MouseButtonReleased && events[1].button == MouseButton::Right),
                  "an injected mouse button keeps its button and direction");
    CHECK_MESSAGE(events[2].kind == InputEventKind::WindowFocusLost, "an injected focus keeps its direction");
    CHECK_MESSAGE(events[3].kind == InputEventKind::WindowFocusGained, "an injected focus keeps its direction");
}

TEST_CASE("Injected motion accumulates while the window is focused") {
    auto platform = MakeInjectionPlatform();
    SdlInput input(*platform);
    SdlInputInjector injector(*platform);
    REQUIRE(injector.PushFocus(true));
    PumpFrame(*platform, input);  // the focus gain suppresses this frame's motion
    REQUIRE(injector.PushMouseMotion(3.0F, 7.0F));
    PumpFrame(*platform, input);
    CHECK_MESSAGE((input.Snapshot().pointer.deltaX == 3.0F && input.Snapshot().pointer.deltaY == 7.0F),
                  "an injected motion keeps its x and y");
}

TEST_CASE("Injection fails a GYO_ASSERT inside a live frame") {
    auto platform = MakeInjectionPlatform();
    SdlInputInjector injector(*platform);
    bool asserted = false;
    int runs = 0;
    platform->SetLiveFrameHandler([&] {
        ++runs;
        Engine::Test::ScopedAssertionHandler handler;
        try {
            static_cast<void>(injector.PushKey(Key::W, true));
        } catch (const Engine::Base::AssertionFailure&) {
            asserted = true;
        }
    });
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform));
    SDL_Event trigger{};
    trigger.type = SDL_EVENT_USER;
    REQUIRE(SDL_PushEvent(&trigger));
    bool pushed = false;
    (void)platform->PumpEvents([&](const PlatformEvent& view) {
        if (Engine::Platform::Sdl::NativeEvent(view).type != SDL_EVENT_USER || pushed) return;
        pushed = true;
        SDL_Event exposed{};
        exposed.type = SDL_EVENT_WINDOW_EXPOSED;
        exposed.window.windowID = window;
        exposed.window.data1 = 1;
        SDL_PushEvent(&exposed);
    });
    platform->SetLiveFrameHandler({});
    REQUIRE(runs == 1);
    CHECK_MESSAGE(asserted, "injection inside a live frame fails a GYO_ASSERT");
}
