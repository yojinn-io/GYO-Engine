include(FetchContent)
find_package(Git REQUIRED)

set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

# GYO's patches to SDL are listed in docs/architecture/platform-sdl.md
# ("SDL patches"), each with the condition for removing it.
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.0
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND}
                   -DSOURCE_DIR=<SOURCE_DIR>
                   -DPATCH_DIR=${CMAKE_CURRENT_LIST_DIR}/patches
                   -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                   -P ${CMAKE_CURRENT_LIST_DIR}/ApplyPatches.cmake
)
FetchContent_MakeAvailable(SDL3)

# A source given through FETCHCONTENT_SOURCE_DIR_SDL3 skips the patch step;
# building it unpatched would bring the fixed defects back silently.
file(STRINGS "${sdl3_SOURCE_DIR}/src/video/cocoa/SDL_cocoakeyboard.m" gyo_sdl3_patched
    REGEX "gyo-cocoa-ime-composing-keydown" LIMIT_COUNT 1)
if(NOT gyo_sdl3_patched)
    message(FATAL_ERROR
        "The SDL source at ${sdl3_SOURCE_DIR} lacks GYO's patches; apply "
        "${CMAKE_CURRENT_LIST_DIR}/patches/*.patch to it (see docs/architecture/platform-sdl.md, \"SDL patches\").")
endif()
