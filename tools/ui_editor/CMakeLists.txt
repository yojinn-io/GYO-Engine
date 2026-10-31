cmake_minimum_required(VERSION 3.30)

# The standalone entry forwards to the repository's one engine build graph.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT DEFINED GYO_CURRENT_TOOL)
    project(GyoUiEditor LANGUAGES NONE)
    option(BUILD_TESTING "Build engine and editor tests" OFF)
    include(CTest)
    set(GYO_APPS "" CACHE STRING "Games selected for this build" FORCE)
    set(GYO_TOOLS "ui_editor" CACHE STRING "Tool selection, optionally ui_editor:cli")
    get_filename_component(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    add_subdirectory("${GYO_REPOSITORY_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/repository")
    return()
endif()

set(GYO_UI_EDITOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include("${GYO_REPOSITORY_ROOT}/build/cmake/GyoRuntimeDeployment.cmake")

add_library(gyo_ui_editor_core STATIC
    src/CommandLine.cpp
    src/DocumentSession.cpp
    src/EditorDocumentEdits.cpp
    src/ElementClipboard.cpp
    src/ElementSchema.cpp
    src/GizmoMath.cpp
    src/UiDocumentBridge.cpp
    src/UndoStack.cpp
)

target_include_directories(gyo_ui_editor_core PUBLIC
    "${GYO_UI_EDITOR_ROOT}/include"
)
target_compile_features(gyo_ui_editor_core PUBLIC cxx_std_20)
# File access, the read-only catalog mount and the dialog types come from the
# framework layer (docs/framework/architecture.md).
target_link_libraries(gyo_ui_editor_core
    PUBLIC
        GYO::Engine
        GYO::ToolFramework
        GYO::Ui
        nlohmann_json::nlohmann_json
)
# UndoStack calls Math functions directly; the catalog paths go to UTF-8 through GYO::Os.
target_link_libraries(gyo_ui_editor_core PRIVATE GYO::Math GYO::Os)

add_executable(gyo_ui_editor src/Main.cpp)
target_compile_features(gyo_ui_editor PRIVATE cxx_std_20)
# The UTF-8 command line and console output come from GYO::Os.
target_link_libraries(gyo_ui_editor PRIVATE gyo_ui_editor_core GYO::Os)

if(GYO_TOOL_VARIANT STREQUAL "gui")
    foreach(_target IN ITEMS
        GYO::ToolFrameworkGui
        GYO::AssetSdlImage
        GYO::TextBackendSDLTTF
        imgui::core
    )
        if(NOT TARGET ${_target})
            message(FATAL_ERROR
                "gyo_ui_editor GUI requires the ${_target} target")
        endif()
    endforeach()
    add_library(gyo_ui_editor_preview STATIC
        src/AssetPreviewContext.cpp
        src/EditorApp.cpp
        src/EditorAppCanvas.cpp
        src/EditorAppDialogs.cpp
        src/EditorAppHierarchy.cpp
        src/EditorAppInspector.cpp
        src/PreviewAdapter.cpp
    )
    target_link_libraries(gyo_ui_editor_preview PUBLIC
        gyo_ui_editor_core
        SDL3::SDL3
        GYO::AssetSdlImage
        GYO::TextBackendSDLTTF
        imgui::core
    )
    # PreviewAdapter calls Math functions directly (sRGB, Rect); EditorApp's
    # path fields go to UTF-8 through GYO::Os.
    target_link_libraries(gyo_ui_editor_preview PRIVATE GYO::Math GYO::Os)
    target_sources(gyo_ui_editor PRIVATE src/EditorWindow.cpp src/SdlPathDialogs.cpp)
    # The window, ImGui's SDL backends and the system dialogs come from the
    # framework's host.
    target_link_libraries(gyo_ui_editor PRIVATE
        gyo_ui_editor_preview
        GYO::ToolFrameworkGui
    )
    target_compile_definitions(gyo_ui_editor PRIVATE
        GYO_UI_EDITOR_WITH_GUI=1
    )


else()
    target_sources(gyo_ui_editor PRIVATE src/EditorGuiUnavailable.cpp)
    target_compile_definitions(gyo_ui_editor PRIVATE
        GYO_UI_EDITOR_WITH_GUI=0
    )
endif()

if(MSVC)
    target_compile_options(gyo_ui_editor_core PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(gyo_ui_editor PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_ui_editor_core PRIVATE NOMINMAX)
    target_compile_definitions(gyo_ui_editor PRIVATE NOMINMAX)
    if(TARGET gyo_ui_editor_preview)
        target_compile_options(gyo_ui_editor_preview PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(gyo_ui_editor_preview PRIVATE NOMINMAX)
    endif()
else()
    target_compile_options(gyo_ui_editor_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(gyo_ui_editor PRIVATE -Wall -Wextra -Wpedantic)
    if(TARGET gyo_ui_editor_preview)
        target_compile_options(gyo_ui_editor_preview PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

gyo_tool_install_target(TARGET gyo_ui_editor ROLE main)
