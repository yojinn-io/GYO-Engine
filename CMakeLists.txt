cmake_minimum_required(VERSION 3.30)
project(GYO-Engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GYO_BUILD_RUNTIME "Build the minimal standalone runtime application" OFF)
option(GYO_BUILD_SANDBOX "Build the optional ImGui/JSON sandbox application" OFF)
option(GYO_BUILD_OBJECT_FPS "Build the Object_FPS GYO conformance game" ON)
option(GYO_BUILD_UI_EDITOR "Build the standalone GYO JSON UI editor" ON)
option(
    GYO_BUILD_SDL_GPU_BACKEND
    "Build the optional Windows/MSVC SDL_GPU render backend"
    OFF
)
option(
    GYO_BUILD_SDL_TTF_ADAPTER
    "Build the optional SDL_ttf-backed text rasterizer"
    OFF
)

include(CTest)

# Runtime DLL deployment is shared by applications and standalone tools.  Keep
# it at the composition root so neither side needs to depend on the other.
function(gyo_copy_runtime_dlls target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}>
                $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

# Keep dependency sources private to each build tree. Disconnected updates still
# allow the first population, but do not mutate an already populated dependency.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL
    "Do not update already-populated FetchContent dependencies")

add_subdirectory(third_party)
add_subdirectory(engine)
add_subdirectory(text)
add_subdirectory(render)
add_subdirectory(ui)

if(GYO_BUILD_RUNTIME OR GYO_BUILD_SANDBOX OR GYO_BUILD_OBJECT_FPS OR
   GYO_BUILD_SDL_GPU_BACKEND OR GYO_BUILD_UI_EDITOR)
    add_subdirectory(platform)
endif()

if(GYO_BUILD_RUNTIME OR GYO_BUILD_SANDBOX OR GYO_BUILD_OBJECT_FPS)
    add_subdirectory(input)
    add_subdirectory(apps)
endif()

if(GYO_BUILD_UI_EDITOR)
    add_subdirectory(tools)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
