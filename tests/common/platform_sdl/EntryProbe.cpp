// A program on GYO::PlatformSDLMain for the entry point and death tests: exits
// with the code given as argument, throws on "throw", and on "replaced-bridge"
// destroys an SDL log bridge whose output was replaced.
#include "engine/base/Assert.hpp"
#include "engine/platform/sdl/EntryPoint.hpp"
#include "engine/platform/sdl/SdlLogBridge.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {

void SDLCALL Other(void*, int, SDL_LogPriority, const char*) {}

// Exit codes of the "replaced-bridge" death test (EntryTests.cpp).
constexpr int AssertedOtherIntact = 41;
constexpr int AssertedOtherOverwritten = 42;

// Runs at the bridge's GYO_ASSERT: reports whether the other output is still
// in place and ends the process (a handler that returned would abort).
[[noreturn]] void ReportAssertion(const Engine::Base::AssertionFailure&) {
    SDL_LogOutputFunction current{};
    void* data{};
    SDL_GetLogOutputFunction(&current, &data);
    std::_Exit(current == &Other ? AssertedOtherIntact : AssertedOtherOverwritten);
}

// Exit code of the "watch-in-live-frame" death test: the watch's destructor
// failed its GYO_ASSERT.
constexpr int WatchAsserted = 43;

[[noreturn]] void ReportWatchAssertion(const Engine::Base::AssertionFailure&) {
    std::_Exit(WatchAsserted);
}

void IgnoreEvent(void*, const Engine::Platform::Sdl::ObservedEvent&) noexcept {}

// Destroys an event watch inside a live frame (Design Invariant 7).
int DestroyWatchInLiveFrame() {
    auto created = Engine::Platform::Sdl::SdlPlatform::Create();
    if (!created) return 2;
    auto& platform = *created.value();
    Engine::Base::SetAssertionHandler(&ReportWatchAssertion);
    auto watch = platform.AddEventWatch(&IgnoreEvent, nullptr);
    platform.SetLiveFrameHandler([&] { watch = Engine::Platform::Sdl::EventWatch(); });
    const SDL_WindowID window = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(platform));
    SDL_Event trigger{};
    trigger.type = SDL_EVENT_USER;
    SDL_PushEvent(&trigger);
    bool pushed = false;
    static_cast<void>(platform.PumpEvents([&](const Engine::Platform::Sdl::PlatformEvent& view) {
        if (Engine::Platform::Sdl::NativeEvent(view).type != SDL_EVENT_USER || pushed) return;
        pushed = true;
        SDL_Event exposed{};
        exposed.type = SDL_EVENT_WINDOW_EXPOSED;
        exposed.window.windowID = window;
        exposed.window.data1 = 1;
        SDL_PushEvent(&exposed);
    }));
    platform.SetLiveFrameHandler({});
    return 0;  // no assertion
}

// Destroys an event watch inside another watch's callback (Design Invariant 7).
struct CallbackVictim final {
    Engine::Platform::Sdl::EventWatch victim;
};

void DestroyVictim(void* context, const Engine::Platform::Sdl::ObservedEvent&) noexcept {
    static_cast<CallbackVictim*>(context)->victim = Engine::Platform::Sdl::EventWatch();
}

int DestroyWatchInCallback() {
    auto created = Engine::Platform::Sdl::SdlPlatform::Create();
    if (!created) return 2;
    auto& platform = *created.value();
    Engine::Base::SetAssertionHandler(&ReportWatchAssertion);
    CallbackVictim state;
    state.victim = platform.AddEventWatch(&IgnoreEvent, nullptr);
    const auto destroyer = platform.AddEventWatch(&DestroyVictim, &state);
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(platform));
    SDL_PushEvent(&motion);
    return 0;  // no assertion
}

// Destroys an event watch on another thread (Design Invariant 7).
int DestroyWatchOffMainThread() {
    auto created = Engine::Platform::Sdl::SdlPlatform::Create();
    if (!created) return 2;
    auto& platform = *created.value();
    Engine::Base::SetAssertionHandler(&ReportWatchAssertion);
    auto watch = platform.AddEventWatch(&IgnoreEvent, nullptr);
    std::thread other([moved = std::move(watch)]() mutable { moved = Engine::Platform::Sdl::EventWatch(); });
    other.join();
    return 0;  // no assertion
}

} // namespace

int GyoMain(int argc, char* argv[]) {
    if (argc > 1 && std::string_view(argv[1]) == "watch-in-live-frame") return DestroyWatchInLiveFrame();
    if (argc > 1 && std::string_view(argv[1]) == "watch-in-callback") return DestroyWatchInCallback();
    if (argc > 1 && std::string_view(argv[1]) == "watch-off-main") return DestroyWatchOffMainThread();
    if (argc > 1 && std::string_view(argv[1]) == "throw") throw std::runtime_error("entry-probe");
    if (argc > 1 && std::string_view(argv[1]) == "replaced-bridge") {
        Engine::Base::SetAssertionHandler(&ReportAssertion);
        {
            const Engine::Platform::Sdl::SdlLogBridge bridge;
            SDL_SetLogOutputFunction(&Other, nullptr);
        }
        return 0;  // no assertion
    }
    return argc > 1 ? std::atoi(argv[1]) : 0;
}
