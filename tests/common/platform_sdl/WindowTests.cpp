// IP-4 ip4a: window and display queries and window operations
// (ip4-declaration.md section 1).
#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <optional>

using Engine::Platform::Sdl::PlatformEvent;
using Engine::Platform::Sdl::SdlPlatform;

namespace {

std::unique_ptr<SdlPlatform> MakeWindowPlatform() {
    auto platform = SdlPlatform::Create();
    REQUIRE(platform);
    return std::move(platform.value());
}

// Runs body inside the observer of one pump.
template <class Body>
void InsideObserver(SdlPlatform& platform, Body body) {
    SDL_Event trigger{};
    trigger.type = SDL_EVENT_USER;
    trigger.user.code = 4242;
    REQUIRE(SDL_PushEvent(&trigger));
    bool ran = false;
    (void)platform.PumpEvents([&](const PlatformEvent& view) {
        const SDL_Event& event = Engine::Platform::Sdl::NativeEvent(view);
        if (event.type != SDL_EVENT_USER || event.user.code != 4242 || ran) return;
        ran = true;
        body();
    });
    REQUIRE(ran);
}

} // namespace

TEST_CASE("QueryWindow reads the window from SDL every time") {
    auto platform = MakeWindowPlatform();
    const auto before = platform->QueryWindow();
    REQUIRE((before.rect.width > 0 && before.rect.width != 640));
    REQUIRE(SDL_SetWindowSize(Engine::Platform::Sdl::NativeWindow(*platform), 640, 360));
    const auto state = platform->QueryWindow();
    CHECK_MESSAGE((state.rect.width == 640 && state.rect.height == 360), "a query reads the size SDL has now");
    REQUIRE(SDL_SetWindowRelativeMouseMode(Engine::Platform::Sdl::NativeWindow(*platform), true));
    CHECK_MESSAGE(platform->QueryWindow().flags.relativeMouseMode, "a query reads the relative mouse mode SDL has now");
    REQUIRE(SDL_SetWindowRelativeMouseMode(Engine::Platform::Sdl::NativeWindow(*platform), false));
    CHECK_FALSE(platform->QueryWindow().flags.relativeMouseMode);
}

TEST_CASE("Window operations change the window") {
    auto platform = MakeWindowPlatform();
    REQUIRE(platform->SetWindowSize(800, 450));
    const auto size = platform->QueryWindow().rect;
    CHECK((size.width == 800 && size.height == 450));
    REQUIRE(platform->SetWindowPosition(37, 53));
    static_cast<void>(platform->Sync());
    platform->Raise();
    std::optional<std::pair<int, int>> moved;
    (void)platform->PumpEvents([&](const PlatformEvent& view) {
        const SDL_Event& event = Engine::Platform::Sdl::NativeEvent(view);
        if (event.type == SDL_EVENT_WINDOW_MOVED) moved = std::pair{event.window.data1, event.window.data2};
    });
    REQUIRE(moved);
    CHECK((moved->first == 37 && moved->second == 53));
}

TEST_CASE("Queries may run inside a live frame") {
    auto platform = MakeWindowPlatform();
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(*platform));
    bool asserted = false;
    int width = 0;
    platform->SetLiveFrameHandler([&] {
        Engine::Test::ScopedAssertionHandler handler;
        try {
            width = platform->QueryWindow().rect.width;
        } catch (const Engine::Base::AssertionFailure&) {
            asserted = true;
        }
    });
    InsideObserver(*platform, [&] {
        SDL_Event exposed{};
        exposed.type = SDL_EVENT_WINDOW_EXPOSED;
        exposed.window.windowID = window;
        exposed.window.data1 = 1;
        SDL_PushEvent(&exposed);
    });
    platform->SetLiveFrameHandler({});
    CHECK_MESSAGE(!asserted, "a query inside a live frame does not fail an assertion");
    CHECK(width > 0);
}

TEST_CASE("Window operations fail a GYO_ASSERT inside an observer") {
    auto platform = MakeWindowPlatform();
    InsideObserver(*platform, [&] {
        GYO_CHECK_ASSERTS(platform->SetWindowSize(320, 240));
        GYO_CHECK_ASSERTS(platform->SetWindowPosition(1, 2));
        GYO_CHECK_ASSERTS(platform->Raise());
        GYO_CHECK_ASSERTS(platform->Sync());
    });
}

TEST_CASE("Display and driver queries describe the dummy platform") {
    auto platform = MakeWindowPlatform();
    const auto display = platform->QueryDisplay();
    REQUIRE(display);
    CHECK((display.value().usableBounds.width > 0 && display.value().usableBounds.height > 0));
    CHECK(display.value().refreshRate >= 0.0F);
    CHECK(platform->VideoDriverName() == "dummy");
    CHECK_MESSAGE(!platform->X11WindowNumber().has_value(), "a window that is not an X11 window has no X11 number");
}
