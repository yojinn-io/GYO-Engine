// A program on GYO::PlatformSDLMain for the entry point and death tests: exits
// with the code given as argument, throws on "throw", and on "replaced-bridge"
// destroys an SDL log bridge whose output was replaced.
#include "engine/base/Assert.hpp"
#include "engine/platform/sdl/EntryPoint.hpp"
#include "engine/platform/sdl/SdlLogBridge.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <stdexcept>
#include <string_view>

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

} // namespace

int GyoMain(int argc, char* argv[]) {
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
