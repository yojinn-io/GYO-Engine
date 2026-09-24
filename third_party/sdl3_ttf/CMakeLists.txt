include(FetchContent)

# SDL_ttf is selected only by the optional text adapter. Keep the dependency
# static and trim authoring/sample features that the runtime does not use.
set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLTTF_WERROR OFF CACHE BOOL "" FORCE)
set(SDLTTF_STRICT ON CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)

if(DEFINED BUILD_SHARED_LIBS)
    set(_gyo_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(_gyo_restore_build_shared_libs TRUE)
else()
    set(_gyo_restore_build_shared_libs FALSE)
endif()
set(BUILD_SHARED_LIBS OFF)

FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        release-3.2.2
    # Only FreeType is enabled above. Avoid cloning disabled shaping/SVG
    # dependencies into the build tree (or, on a failed retry, the source tree).
    GIT_SUBMODULES external/freetype
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(SDL3_ttf)

# Upstream's static target exports only SDL headers and lets its consumer choose
# the SDL library. GYO has already selected SDL3::SDL3; declare that real link
# dependency here so one-pass linkers place its import/archive after SDL_ttf.
if(TARGET SDL3_ttf-static)
    target_link_libraries(SDL3_ttf-static PRIVATE SDL3::SDL3)
endif()

if(_gyo_restore_build_shared_libs)
    set(BUILD_SHARED_LIBS "${_gyo_saved_build_shared_libs}")
else()
    unset(BUILD_SHARED_LIBS)
endif()
unset(_gyo_restore_build_shared_libs)
unset(_gyo_saved_build_shared_libs)
