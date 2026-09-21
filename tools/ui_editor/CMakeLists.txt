cmake_minimum_required(VERSION 3.30)

# The standalone entry forwards to the repository's one engine build graph.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(GyoUiEditor LANGUAGES NONE)
    option(BUILD_TESTING "Build engine and editor tests" OFF)
    include(CTest)
    set(GYO_APPS "" CACHE STRING "Games selected for this build" FORCE)
    set(GYO_BUILD_UI_EDITOR ON CACHE BOOL "Build the UI editor" FORCE)
    get_filename_component(GYO_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    add_subdirectory("${GYO_REPOSITORY_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/repository")
    return()
endif()

option(GYO_UI_EDITOR_BUILD_GUI "Build the SDL3/ImGui editor host" ON)
if(GYO_ENABLE_PACKAGING AND NOT GYO_UI_EDITOR_BUILD_GUI)
    message(FATAL_ERROR "The toolchain package requires the GUI UI editor; use GUI-OFF only for local CLI builds")
endif()
set(GYO_UI_EDITOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include("${GYO_REPOSITORY_ROOT}/build/cmake/GyoRuntimeDeployment.cmake")

add_library(gyo_ui_editor_core STATIC
    src/CommandLine.cpp
    src/DocumentSession.cpp
    src/FileService.cpp
    src/ReadOnlyAssetCatalog.cpp
    src/UiDocumentBridge.cpp
    src/UndoStack.cpp
)

target_include_directories(gyo_ui_editor_core PUBLIC
    "${GYO_UI_EDITOR_ROOT}/include"
)
target_compile_features(gyo_ui_editor_core PUBLIC cxx_std_20)
target_link_libraries(gyo_ui_editor_core
    PUBLIC
        GYO::Engine
        GYO::Ui
        nlohmann_json::nlohmann_json
)

add_executable(gyo_ui_editor src/Main.cpp)
target_compile_features(gyo_ui_editor PRIVATE cxx_std_20)
target_link_libraries(gyo_ui_editor PRIVATE gyo_ui_editor_core)

if(GYO_UI_EDITOR_BUILD_GUI)
    foreach(_target IN ITEMS
        GYO::PlatformSDL
        GYO::RenderBackendSDL
        GYO::AssetSdlImage
        GYO::TextBackendSDLTTF
        imgui::core
        imgui::sdl3_sdlrenderer3
    )
        if(NOT TARGET ${_target})
            message(FATAL_ERROR
                "gyo_ui_editor GUI requires the ${_target} target")
        endif()
    endforeach()
    add_library(gyo_ui_editor_preview STATIC
        src/AssetPreviewContext.cpp
        src/EditorApp.cpp
        src/PreviewAdapter.cpp
    )
    target_link_libraries(gyo_ui_editor_preview PUBLIC
        gyo_ui_editor_core
        SDL3::SDL3
        GYO::AssetSdlImage
        GYO::TextBackendSDLTTF
        imgui::core
    )
    target_sources(gyo_ui_editor PRIVATE src/ImGuiSdlRendererHost.cpp)
    target_link_libraries(gyo_ui_editor PRIVATE
        gyo_ui_editor_preview
        GYO::PlatformSDL
        GYO::RenderBackendSDL
        imgui::sdl3_sdlrenderer3
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

set(_gyo_editor_components "")
if(GYO_UI_EDITOR_BUILD_GUI)
    set(_gyo_editor_components SDL_RENDERER SDL_IMAGE SDL_TTF IMGUI)
endif()
gyo_install_product_target(TARGET gyo_ui_editor PRODUCT toolchain KIND toolchain
    ROLE ui_editor COMPONENTS ${_gyo_editor_components})
