# The sdl Plugin (config/plugins.csv, library_only): SDL3 for the sdl* Plugins,
# fetched with GYO's patches, and the target GYO::Sdl they depend on
# (docs/architecture/target/v2/sdl.zh-Hant.md). The build layer adds this
# directory only when a configured product carries an sdl* Plugin or
# GYO_PLUGINS names one (Design Invariant SDL-2); without it SDL is neither
# downloaded nor built.
#
# SDL is linked by each sdl* Plugin's own target, PRIVATE, and only that
# Plugin's src/adapters/sdl/ and tests/adapters/sdl/ include SDL3/ (Design
# Invariant SDL-1). GYO::Sdl itself carries no SDL: what the sdl* Plugins share
# (the window register, the clock pairing) is written in GYO's own words.
include(FetchContent)
find_package(Git REQUIRED)

# A static SDL: a product carries no SDL3 shared library next to its program,
# and Windows tests need no DLL search path. SDL's dynamic API stays compiled
# in, so a player can still point SDL3_DYNAMIC_API at another SDL3 build.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)

# The release archive of SDL 3.4.0 (tag release-3.4.0, commit
# a962f40bbba175e9716557a25d5d7965f134a3d3), checked against its SHA-256. The
# patches are written against this version: moving it means applying them
# again (or dropping one SDL fixed), per the table in sdl.zh-Hant.md.
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.0/SDL3-3.4.0.tar.gz
    URL_HASH SHA256=082cbf5f429e0d80820f68dc2b507a94d4cc1b4e70817b119bbb8ec6a69584b8
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PATCH_COMMAND "${CMAKE_COMMAND}"
                  -DSOURCE_DIR=<SOURCE_DIR>
                  "-DPATCH_DIR=${CMAKE_CURRENT_LIST_DIR}/patches"
                  "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                  -P "${CMAKE_CURRENT_LIST_DIR}/cmake/ApplyPatches.cmake"
)
FetchContent_MakeAvailable(SDL3)

# A source given through FETCHCONTENT_SOURCE_DIR_SDL3 skips the patch step;
# building it unpatched would bring the fixed defects back silently.
file(STRINGS "${sdl3_SOURCE_DIR}/src/video/cocoa/SDL_cocoakeyboard.m" gyo_sdl3_patched
    REGEX "gyo-cocoa-ime-composing-keydown" LIMIT_COUNT 1)
if(NOT gyo_sdl3_patched)
    message(FATAL_ERROR
        "The SDL source at ${sdl3_SOURCE_DIR} lacks GYO's patches; apply "
        "${CMAKE_CURRENT_LIST_DIR}/patches/*.patch to it (docs/architecture/target/v2/sdl.zh-Hant.md, SDL patches).")
endif()

add_library(gyo_sdl INTERFACE)
add_library(GYO::Sdl ALIAS gyo_sdl)
target_compile_features(gyo_sdl INTERFACE cxx_std_20)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
