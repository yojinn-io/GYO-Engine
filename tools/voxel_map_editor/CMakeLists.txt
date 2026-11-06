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
)
target_include_directories(gyo_voxel_map_editor_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_voxel_map_editor_core PUBLIC cxx_std_20)
# The baked file and its only validator are Engine's (GYO::VoxelMesh); files go
# through engine/io (GYO::Engine); gyo.voxel_grid is the tool's own format, read
# with the tool's own JSON parser; markers and their kinds only through
# GYO::MapMarkers (docs/voxel_map_editor/architecture.md, Design Invariant 1).
target_link_libraries(gyo_voxel_map_editor_core
    PUBLIC GYO::Base GYO::Engine GYO::VoxelMesh GYO::MapMarkers
    PRIVATE nlohmann_json::nlohmann_json
)

add_executable(gyo_voxel_map_editor src/Main.cpp src/MapMeasure.cpp)
target_compile_features(gyo_voxel_map_editor PRIVATE cxx_std_20)
# The UTF-8 command line and console come from the framework's headless shell.
# The schema of a file to inspect is read with the tool's JSON parser too.
target_link_libraries(gyo_voxel_map_editor PRIVATE gyo_voxel_map_editor_core GYO::ToolFramework
                                                   nlohmann_json::nlohmann_json)
# Local-only tools go to build/target/_tools/<owner>/bin (docs/tool_projects.md, step 3).
gyo_tool_install_target(TARGET gyo_voxel_map_editor ROLE main)

foreach(_target IN ITEMS gyo_voxel_map_editor_core gyo_voxel_map_editor)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${_target} PRIVATE NOMINMAX)
    else()
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
