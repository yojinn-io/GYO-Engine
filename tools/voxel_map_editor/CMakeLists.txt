cmake_minimum_required(VERSION 3.30)

# The standalone entry forwards to the repository's one engine build graph.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT DEFINED GYO_CURRENT_TOOL)
    project(GyoVoxelMapEditor LANGUAGES NONE)
    option(BUILD_TESTING "Build engine and editor tests" OFF)
    include(CTest)
    set(GYO_APPS "" CACHE STRING "Games selected for this build" FORCE)
    set(GYO_TOOLS "voxel_map_editor" CACHE STRING "Tool selection")
    get_filename_component(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    add_subdirectory("${GYO_REPOSITORY_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/repository")
    return()
endif()

add_library(gyo_voxel_map_editor_core STATIC
    src/Orientation.cpp
    src/GridDocument.cpp
    src/Build.cpp
    src/MapBuild.cpp
    src/MarkersFile.cpp
    src/BoxImport.cpp
    src/FileAccess.cpp
    src/Editing.cpp
    src/Tools.cpp
    src/Synthetic.cpp
)
target_include_directories(gyo_voxel_map_editor_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_voxel_map_editor_core PUBLIC cxx_std_20)
# The baked file and its only validator are Engine's (GYO::VoxelMesh); files go
# through engine/io (GYO::Engine); gyo.voxel_grid is the tool's own format, read
# with the tool's own JSON parser; markers and their kinds only through
# GYO::MapMarkers (docs/voxel_map_editor/architecture.md, Design Invariant 1).
# Picking uses Engine's math (GYO::Math), the same as Engine's Renderer
# composes its camera with.
target_link_libraries(gyo_voxel_map_editor_core
    PUBLIC GYO::Base GYO::Engine GYO::Math GYO::VoxelMesh GYO::MapMarkers
    PRIVATE nlohmann_json::nlohmann_json
)

add_executable(gyo_voxel_map_editor src/Main.cpp src/MapMeasure.cpp)
target_compile_features(gyo_voxel_map_editor PRIVATE cxx_std_20)
target_include_directories(gyo_voxel_map_editor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# The UTF-8 command line and console come from the framework's headless shell.
# The schema of a file to inspect is read with the tool's JSON parser too.
target_link_libraries(gyo_voxel_map_editor PRIVATE gyo_voxel_map_editor_core GYO::ToolFramework
                                                   nlohmann_json::nlohmann_json)
set(_gyo_voxel_map_editor_targets gyo_voxel_map_editor_core gyo_voxel_map_editor)
if(GYO_TOOL_VARIANT STREQUAL "gui")
    foreach(_target IN ITEMS GYO::ToolFrameworkPreview GYO::ToolFrameworkImGui GYO::RenderBackendSDL
                             GYO::PlatformSDL imgui::core)
        if(NOT TARGET ${_target})
            message(FATAL_ERROR "gyo_voxel_map_editor GUI requires the ${_target} target")
        endif()
    endforeach()
    # The window (src/window/, Design Invariant 1): the map drawn by Engine on
    # the framework's offscreen preview (docs/framework/preview.md), shown as a
    # texture of the ImGui shell's SDL renderer, so no *Native target.
    add_library(gyo_voxel_map_editor_window STATIC
        src/window/PreviewScene.cpp
        src/window/EditorWindow.cpp
        src/window/PreviewMeasure.cpp
    )
    target_include_directories(gyo_voxel_map_editor_window PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(gyo_voxel_map_editor_window
        PUBLIC gyo_voxel_map_editor_core
        PRIVATE GYO::ToolFrameworkPreview GYO::ToolFrameworkImGui GYO::RenderBackendSDL GYO::PlatformSDL imgui::core
    )
    add_dependencies(gyo_voxel_map_editor_window gyo_builtin_shaders)
    target_link_libraries(gyo_voxel_map_editor PRIVATE gyo_voxel_map_editor_window)
    # Engine's builtin shaders next to the executable, where the window looks;
    # copied on every build so a shader-only change reaches it too.
    add_custom_target(gyo_voxel_map_editor_shaders
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${PROJECT_BINARY_DIR}/shaders/builtin"
                "$<TARGET_FILE_DIR:gyo_voxel_map_editor>/shaders/builtin"
        DEPENDS gyo_builtin_shaders
        VERBATIM)
    add_dependencies(gyo_voxel_map_editor gyo_voxel_map_editor_shaders)
    list(APPEND _gyo_voxel_map_editor_targets gyo_voxel_map_editor_window)
else()
    target_sources(gyo_voxel_map_editor PRIVATE src/window/EditorWindowUnavailable.cpp)
endif()
# Local-only tools go to build/target/_tools/<owner>/bin (docs/tool_projects.md, step 3).
gyo_tool_install_target(TARGET gyo_voxel_map_editor ROLE main)

foreach(_target IN LISTS _gyo_voxel_map_editor_targets)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${_target} PRIVATE NOMINMAX)
    else()
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
