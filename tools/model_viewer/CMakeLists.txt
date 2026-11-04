cmake_minimum_required(VERSION 3.30)

# The standalone entry forwards to the repository's one engine build graph.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT DEFINED GYO_CURRENT_TOOL)
    project(GyoModelViewer LANGUAGES NONE)
    option(BUILD_TESTING "Build engine and viewer tests" OFF)
    include(CTest)
    set(GYO_APPS "" CACHE STRING "Games selected for this build" FORCE)
    set(GYO_TOOLS "model_viewer" CACHE STRING "Tool selection")
    get_filename_component(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    add_subdirectory("${GYO_REPOSITORY_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/repository")
    return()
endif()

add_library(gyo_model_viewer_core STATIC
    src/EventDocument.cpp
    src/EventFiles.cpp
    src/FileAccess.cpp
    src/ModelContent.cpp
    src/ModelSummary.cpp
)
target_include_directories(gyo_model_viewer_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_model_viewer_core PUBLIC cxx_std_20)
# Models and gyo.anim_events are Engine's (GYO::Model, its ufbx loader); files
# go through engine/io (GYO::Engine); the catalog is mounted by the framework
# and a new entry written by its JSON binding (docs/model_viewer/architecture.md,
# Design Invariant 1).
target_link_libraries(gyo_model_viewer_core
    PUBLIC GYO::Base GYO::Engine GYO::Model GYO::ToolFramework
    PRIVATE GYO::AssetUfbx GYO::ToolFrameworkJson
)

add_executable(gyo_model_viewer src/Main.cpp)
target_compile_features(gyo_model_viewer PRIVATE cxx_std_20)
# The UTF-8 command line and console come from the framework's headless shell.
target_link_libraries(gyo_model_viewer PRIVATE gyo_model_viewer_core GYO::ToolFramework)

if(MSVC)
    foreach(_target IN ITEMS gyo_model_viewer_core gyo_model_viewer)
        target_compile_options(${_target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${_target} PRIVATE NOMINMAX)
    endforeach()
else()
    foreach(_target IN ITEMS gyo_model_viewer_core gyo_model_viewer)
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()

gyo_tool_install_target(TARGET gyo_model_viewer ROLE main)
