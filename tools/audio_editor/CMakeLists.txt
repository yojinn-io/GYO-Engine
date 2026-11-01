cmake_minimum_required(VERSION 3.30)

# The standalone entry forwards to the repository's one engine build graph.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT DEFINED GYO_CURRENT_TOOL)
    project(GyoAudioEditor LANGUAGES NONE)
    option(BUILD_TESTING "Build engine and editor tests" OFF)
    include(CTest)
    set(GYO_APPS "" CACHE STRING "Games selected for this build" FORCE)
    set(GYO_TOOLS "audio_editor" CACHE STRING "Tool selection")
    get_filename_component(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    add_subdirectory("${GYO_REPOSITORY_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/repository")
    return()
endif()

add_library(gyo_audio_editor_core STATIC
    src/Conversion.cpp
    src/SoundExport.cpp
    src/WavFile.cpp
)
target_include_directories(gyo_audio_editor_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_audio_editor_core PUBLIC cxx_std_20)
# The clip format comes from GYO::Audio (Design Invariant 2); files go through
# GYO::Os; the catalog entry is written by the framework layer
# (docs/framework/architecture.md).
target_link_libraries(gyo_audio_editor_core
    PUBLIC GYO::Base
    PRIVATE GYO::Audio GYO::Os GYO::ToolFramework
)
# Reproducible conversion (Design Invariant 7): no fused multiply-add, so a
# compiler may not round the filter differently from the plain expression.
if(NOT MSVC)
    set_source_files_properties(src/Conversion.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
endif()

add_executable(gyo_audio_editor src/Main.cpp)
target_compile_features(gyo_audio_editor PRIVATE cxx_std_20)
target_link_libraries(gyo_audio_editor PRIVATE gyo_audio_editor_core GYO::Os)

if(MSVC)
    foreach(_target IN ITEMS gyo_audio_editor_core gyo_audio_editor)
        target_compile_options(${_target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${_target} PRIVATE NOMINMAX)
    endforeach()
else()
    foreach(_target IN ITEMS gyo_audio_editor_core gyo_audio_editor)
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()

gyo_tool_install_target(TARGET gyo_audio_editor ROLE main)
