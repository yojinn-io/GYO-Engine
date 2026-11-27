// The unattended SDL assertion handler of the test executables (UnattendedAssertions.hpp). SDL's own mechanism:
// SDL_SetAssertionHandler (SDL_assert.h:589); SDL_Quit keeps it (SDL_AssertionsQuit only reports, SDL_assert.c:387).
// Returning SDL_ASSERTION_ABORT makes SDL quit and abort (SDL_AbortAssertion, SDL_assert.c:123-127): the test run fails.
#include "UnattendedAssertions.hpp"

#include <SDL3/SDL_assert.h>

#include <cstdio>

namespace GYO::Plugins::SDL {

namespace {

SDL_AssertState SDLCALL FailUnattended(const SDL_AssertData* data, void*) {
    std::fprintf(stderr, "SDL assertion failed (unattended, aborting): %s at %s:%d in %s\n", data->condition, data->filename,
                 data->linenum, data->function);
    std::fflush(stderr);
    return SDL_ASSERTION_ABORT;
}

// Before main: before any test starts SDL.
const bool installed = [] {
    SDL_SetAssertionHandler(&FailUnattended, nullptr);
    return true;
}();

} // namespace

bool UnattendedAssertionsInstalled() noexcept {
    void* userdata = nullptr;
    return installed && SDL_GetAssertionHandler(&userdata) == &FailUnattended;
}

} // namespace GYO::Plugins::SDL
