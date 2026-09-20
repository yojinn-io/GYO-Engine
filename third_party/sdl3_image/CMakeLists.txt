include(FetchContent)

# The current image capability provides PNG decoding. Keep SDL_image optional
# and avoid pulling unrelated codec stacks into the runtime build.
set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WERROR OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_STRICT ON CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)

set(SDLIMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
set(SDLIMAGE_BACKEND_WIC OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "" FORCE)

set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_BMP OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_GIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JPG OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_LBM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PCX OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDLIMAGE_PNM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_QOI OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SVG OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TGA OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XCF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XPM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XV OFF CACHE BOOL "" FORCE)

set(SDLIMAGE_AVIF_SAVE OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JPG_SAVE OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PNG_SAVE OFF CACHE BOOL "" FORCE)

if(DEFINED BUILD_SHARED_LIBS)
    set(_gyo_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(_gyo_restore_build_shared_libs TRUE)
else()
    set(_gyo_restore_build_shared_libs FALSE)
endif()
set(BUILD_SHARED_LIBS OFF)

FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        release-3.2.6
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE
)
FetchContent_MakeAvailable(SDL3_image)

if(_gyo_restore_build_shared_libs)
    set(BUILD_SHARED_LIBS "${_gyo_saved_build_shared_libs}")
else()
    unset(BUILD_SHARED_LIBS)
endif()
unset(_gyo_restore_build_shared_libs)
unset(_gyo_saved_build_shared_libs)
