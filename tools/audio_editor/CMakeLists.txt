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
    src/Edit.cpp
    src/FileAccess.cpp
    src/PreviewPlayer.cpp
    src/ProjectFile.cpp
    src/SoundExport.cpp
    src/WavFile.cpp
    src/Waveform.cpp
)
target_include_directories(gyo_audio_editor_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_audio_editor_core PUBLIC cxx_std_20)
# The clip format comes from GYO::Audio (Design Invariant 2), whose mixer the
# preview player plays on (its header names the mixer); files are read and
# written through engine/io (GYO::Engine) and path identity comes from GYO::Os;
# the catalog entry is placed by the framework layer and its text rewritten by
# the framework's JSON binding (docs/framework/architecture.md). The project
# file is the editor's own format, read and written with nlohmann_json
# (v8-tool-audio-editor-edit-restart).
target_link_libraries(gyo_audio_editor_core
    PUBLIC GYO::Base GYO::Audio
    PRIVATE GYO::Engine GYO::Os GYO::ToolFramework GYO::ToolFrameworkJson nlohmann_json::nlohmann_json
)
# Reproducible conversion (Design Invariant 7): no fused multiply-add, so a
# compiler may not round the filter differently from the plain expression.
if(NOT MSVC)
    set_source_files_properties(src/Conversion.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
endif()

add_executable(gyo_audio_editor src/Main.cpp)
target_compile_features(gyo_audio_editor PRIVATE cxx_std_20)
# info reads the file through engine/io; the UTF-8 command line and console
# come from the framework's headless shell.
target_link_libraries(gyo_audio_editor PRIVATE gyo_audio_editor_core GYO::Engine GYO::ToolFramework)

if(GYO_TOOL_VARIANT STREQUAL "gui")
    foreach(_target IN ITEMS GYO::ToolFrameworkImGui GYO::AudioBackendSDL imgui::core)
        if(NOT TARGET ${_target})
            message(FATAL_ERROR "gyo_audio_editor GUI requires the ${_target} target")
        endif()
    endforeach()
    # The window, ImGui's SDL backends and the system dialogs come from the
    # framework's ImGui binding (its context lends Engine's platform, which
    # comes with the binding's public link, so the editor does not name it); the
    # preview plays through GYO::Audio's SDL backend, which keeps SDL inside it
    # (Design Invariant 1).
    target_sources(gyo_audio_editor PRIVATE src/AudioEditorWindow.cpp)
    target_link_libraries(gyo_audio_editor PRIVATE GYO::ToolFrameworkImGui GYO::AudioBackendSDL imgui::core)
else()
    target_sources(gyo_audio_editor PRIVATE src/AudioEditorGuiUnavailable.cpp)
endif()

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
