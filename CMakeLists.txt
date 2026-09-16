cmake_minimum_required(VERSION 3.30)
# Set this before project() initializes the Apple toolchain. The application
# and offline Metal compiler must share one explicit deployment target.
if((CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR
    (CMAKE_HOST_APPLE AND NOT CMAKE_SYSTEM_NAME)) AND
   (NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL ""))
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS runtime version" FORCE)
endif()
project(GYO-Engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GYO_BUILD_RUNTIME "Build the minimal standalone runtime application" OFF)
option(GYO_BUILD_SANDBOX "Build the optional ImGui/JSON sandbox application" OFF)
option(GYO_BUILD_OBJECT_FPS "Build the Object_FPS GYO conformance game" ON)
option(GYO_BUILD_UI_EDITOR "Build the standalone GYO JSON UI editor" ON)
if(APPLE AND GYO_BUILD_OBJECT_FPS AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "13.3")
    message(FATAL_ERROR "Object_FPS uses macOS 13.3 as its minimum supported deployment target")
endif()
option(
    GYO_BUILD_SDL_GPU_BACKEND
    "Build the optional SDL_GPU render backend (legacy explicit enable)"
    OFF
)
option(
    GYO_BUILD_SDL_TTF_ADAPTER
    "Build the optional SDL_ttf-backed text rasterizer"
    OFF
)

include(CTest)
include(cmake/GyoRenderOptions.cmake)

if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()

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
add_subdirectory(model)
add_subdirectory(collision)
add_subdirectory(text)
add_subdirectory(render)
add_subdirectory(ui)

if(GYO_BUILD_RUNTIME OR GYO_BUILD_SANDBOX OR GYO_BUILD_OBJECT_FPS OR
   GYO_ENABLE_SDL_GPU OR GYO_BUILD_UI_EDITOR)
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
    add_test(NAME cmake.render_options
        COMMAND ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/tests/cmake/RenderOptionsTests.cmake")
    add_test(NAME cmake.msvc_runtime
        COMMAND ${CMAKE_COMMAND} "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/MsvcRuntimeTests.cmake")
endif()

# SDL_image/SDL_ttf and their selected codecs are static. SDL's shared
# runtime is packaged explicitly; platform GPU drivers remain system-owned.
if(TARGET SDL3-shared AND GYO_ENABLE_SDL_GPU)
    install(TARGETS SDL3-shared RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
endif()
if(TARGET gyo_ui_editor)
    install(TARGETS gyo_ui_editor RUNTIME DESTINATION bin)
endif()
if(MSVC AND (TARGET gyo_object_fps OR TARGET gyo_ui_editor))
    # App-local release CRT makes the package usable without Visual Studio.
    # Windows 10+ supplies UCRT; debug CRT is not redistributed.
    include(cmake/GyoMsvcRuntime.cmake)
    gyo_install_msvc_runtime()
endif()

# Every test has an explicit execution class. Backends and shader helpers
# label their tests at registration; existing deterministic tests default CPU.
function(gyo_label_tests directory)
    get_property(_gyo_tests DIRECTORY "${directory}" PROPERTY TESTS)
    foreach(_gyo_test IN LISTS _gyo_tests)
        get_property(_gyo_labels TEST "${_gyo_test}" DIRECTORY "${directory}" PROPERTY LABELS)
        if(NOT _gyo_labels)
            set_property(TEST "${_gyo_test}" DIRECTORY "${directory}" PROPERTY LABELS cpu)
        endif()
    endforeach()
    get_property(_gyo_subdirs DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(_gyo_subdir IN LISTS _gyo_subdirs)
        gyo_label_tests("${_gyo_subdir}")
    endforeach()
endfunction()
if(BUILD_TESTING)
    gyo_label_tests("${CMAKE_CURRENT_SOURCE_DIR}")
endif()
