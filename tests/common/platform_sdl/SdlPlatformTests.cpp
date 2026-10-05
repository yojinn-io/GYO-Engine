#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "engine/platform/sdl/SdlPlatform.hpp"

#include <memory>

using Engine::Platform::Sdl::SdlPlatform;

namespace {

std::unique_ptr<SdlPlatform> MakePlatform() {
    auto platform = SdlPlatform::Create();
    REQUIRE(platform);
    return std::move(platform.value());
}

SDL_Event Exposed(const SDL_WindowID window, const int liveResize) {
    SDL_Event event{};
    event.type = SDL_EVENT_WINDOW_EXPOSED;
    event.window.windowID = window;
    event.window.data1 = liveResize;
    return event;
}

// Pumps once and, while the pump is running (from the observer, which sees a
// trigger event pushed beforehand), pushes the given event. SDL calls event
// watchers while pushing, as it does for its own live-resize redraw requests.
void PushWhilePumping(SdlPlatform& platform, SDL_Event event) {
    SDL_Event trigger{};
    trigger.type = SDL_EVENT_USER;
    REQUIRE(SDL_PushEvent(&trigger));
    bool pushed = false;
    (void)platform.PumpEvents([&](const SDL_Event& seen) {
        if (seen.type == SDL_EVENT_USER && !pushed) {
            pushed = true;
            SDL_PushEvent(&event);
        }
    });
    REQUIRE(pushed);
}

} // namespace

TEST_CASE("SdlPlatform runs the live frame handler for this window's live-resize redraws") {
    auto platform = MakePlatform();
    const SDL_WindowID window = SDL_GetWindowID(platform->NativeWindow());
    int runs = 0;
    platform->SetLiveFrameHandler([&] { ++runs; });

    PushWhilePumping(*platform, Exposed(window, 1));
    CHECK(runs == 1);

    SUBCASE("ordinary redraws, other windows and other events do not run it") {
        PushWhilePumping(*platform, Exposed(window, 0));
        PushWhilePumping(*platform, Exposed(window + 1, 1));
        SDL_Event moved{};
        moved.type = SDL_EVENT_WINDOW_MOVED;
        moved.window.windowID = window;
        moved.window.data1 = 1;
        PushWhilePumping(*platform, moved);
        CHECK(runs == 1);
    }

    SUBCASE("requests outside PumpEvents do not run it") {
        SDL_Event event = Exposed(window, 1);
        REQUIRE(SDL_PushEvent(&event));
        CHECK(runs == 1);
        (void)platform->PumpEvents();  // drains the queued request
        CHECK(runs == 1);
    }

    SUBCASE("an empty handler turns it off") {
        platform->SetLiveFrameHandler({});
        PushWhilePumping(*platform, Exposed(window, 1));
        CHECK(runs == 1);
        platform->SetLiveFrameHandler([&] { ++runs; });
        PushWhilePumping(*platform, Exposed(window, 1));
        CHECK(runs == 2);
    }
}

TEST_CASE("SdlPlatform never runs the live frame handler re-entrantly") {
    auto platform = MakePlatform();
    const SDL_WindowID window = SDL_GetWindowID(platform->NativeWindow());
    int runs = 0;
    int depth = 0;
    int deepest = 0;
    platform->SetLiveFrameHandler([&] {
        ++runs;
        ++depth;
        deepest = depth > deepest ? depth : deepest;
        // A redraw request raised while the handler runs, as drawing can cause.
        SDL_Event again = Exposed(window, 1);
        SDL_PushEvent(&again);
        --depth;
    });

    PushWhilePumping(*platform, Exposed(window, 1));
    CHECK(runs == 1);
    CHECK(deepest == 1);
}

TEST_CASE("SdlPlatform without a live frame handler pumps as before") {
    auto platform = MakePlatform();
    const SDL_WindowID window = SDL_GetWindowID(platform->NativeWindow());
    int seen = 0;
    SDL_Event event = Exposed(window, 1);
    REQUIRE(SDL_PushEvent(&event));
    const auto control = platform->PumpEvents([&](const SDL_Event& e) {
        seen += e.type == SDL_EVENT_WINDOW_EXPOSED ? 1 : 0;
    });
    CHECK(control == Engine::Runtime::RuntimeControl::Continue);
    CHECK(seen == 1);
}
