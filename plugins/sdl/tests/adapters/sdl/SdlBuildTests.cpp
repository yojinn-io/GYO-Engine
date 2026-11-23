// The SDL the sdl Plugin fetches is the one the patches were written for, and
// it starts and stops headless: the dummy video driver needs no display, so
// every CI row runs this (docs/architecture/target/v2/sdl.zh-Hant.md).
#include <SDL3/SDL.h>

#include <doctest/doctest.h>

#include <string_view>

TEST_CASE("build: the linked SDL is the pinned 3.4.0") {
    CHECK(SDL_GetVersion() == SDL_VERSIONNUM(3, 4, 0));
    CHECK(SDL_GetVersion() == SDL_VERSION);
}

TEST_CASE("build: SDL starts and stops with the dummy video driver") {
    REQUIRE(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"));
    REQUIRE_MESSAGE(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
    const char* driver = SDL_GetCurrentVideoDriver();
    REQUIRE(driver != nullptr);
    CHECK(std::string_view(driver) == "dummy");
    SDL_PumpEvents();
    SDL_Quit();
    CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0);
}
