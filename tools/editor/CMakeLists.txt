option(GYO_UI_EDITOR_BUILD_GUI "Build the SDL3/ImGui editor host" ON)
set(_gyo_editor_components "")
if(GYO_UI_EDITOR_BUILD_GUI)
    set(_gyo_editor_components SDL_RENDERER SDL_IMAGE SDL_TTF IMGUI)
endif()
if(COMMAND gyo_app_requirements)
    gyo_app_requirements(COMPONENTS ${_gyo_editor_components})
    if(GYO_APP_DISCOVERY)
        return()
    endif()
endif()

cmake_minimum_required(VERSION 3.30)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(GyoUiEditor LANGUAGES C CXX)
    include(CTest)
endif()

set(GYO_UI_EDITOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(
    GYO_UI_EDITOR_REPOSITORY_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    ABSOLUTE
)

include("${GYO_UI_EDITOR_REPOSITORY_ROOT}/cmake/GyoRuntimeDeployment.cmake")

# Standalone configuration is intentionally local to tools/editor. The normal
# repository integration may provide these targets before adding this folder.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include("${GYO_UI_EDITOR_REPOSITORY_ROOT}/cmake/GyoComponents.cmake")
    gyo_resolve_components(${_gyo_editor_components})
    if(GYO_ENABLE_SDL_GPU)
        message(FATAL_ERROR "The standalone UI editor uses SDLRenderer or CLI mode. Configure the repository root to build SDL_GPU backends.")
    endif()
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
    if(NOT TARGET nlohmann_json::nlohmann_json)
        add_subdirectory(
            "${GYO_UI_EDITOR_REPOSITORY_ROOT}/third_party/nlohmann_json"
            "${CMAKE_CURRENT_BINARY_DIR}/deps/nlohmann_json"
        )
    endif()

    if(GYO_UI_EDITOR_BUILD_GUI)
        if(NOT TARGET SDL3::SDL3)
            add_subdirectory(
                "${GYO_UI_EDITOR_REPOSITORY_ROOT}/third_party/sdl3"
                "${CMAKE_CURRENT_BINARY_DIR}/deps/sdl3"
            )
        endif()
        if(NOT TARGET SDL3_image::SDL3_image)
            add_subdirectory(
                "${GYO_UI_EDITOR_REPOSITORY_ROOT}/third_party/sdl3_image"
                "${CMAKE_CURRENT_BINARY_DIR}/deps/sdl3_image"
            )
        endif()
        if(NOT TARGET SDL3_ttf::SDL3_ttf)
            add_subdirectory(
                "${GYO_UI_EDITOR_REPOSITORY_ROOT}/third_party/sdl3_ttf"
                "${CMAKE_CURRENT_BINARY_DIR}/deps/sdl3_ttf"
            )
        endif()
        if(NOT TARGET imgui::core)
            add_subdirectory(
                "${GYO_UI_EDITOR_REPOSITORY_ROOT}/third_party/imgui"
                "${CMAKE_CURRENT_BINARY_DIR}/deps/imgui"
            )
        endif()
    endif()

    if(NOT TARGET GYO::Engine)
        add_subdirectory(
            "${GYO_UI_EDITOR_REPOSITORY_ROOT}/engine"
            "${CMAKE_CURRENT_BINARY_DIR}/deps/engine"
        )
    endif()

    # Standalone configuration composes the same public GYO layers as the
    # repository root, while keeping dependency test suites out of this tool's
    # focused test build.
    set(_gyo_ui_editor_saved_build_testing "${BUILD_TESTING}")
    set(BUILD_TESTING OFF)
    if(GYO_ENABLE_SDL_PLATFORM AND NOT TARGET GYO::PlatformSDL)
        add_subdirectory(
            "${GYO_UI_EDITOR_REPOSITORY_ROOT}/platform/sdl"
            "${CMAKE_CURRENT_BINARY_DIR}/deps/platform_sdl"
        )
    endif()
    if(NOT TARGET GYO::Text)
        add_subdirectory(
            "${GYO_UI_EDITOR_REPOSITORY_ROOT}/text"
            "${CMAKE_CURRENT_BINARY_DIR}/deps/text"
        )
    endif()
    if(NOT TARGET GYO::Render)
        add_subdirectory(
            "${GYO_UI_EDITOR_REPOSITORY_ROOT}/render"
            "${CMAKE_CURRENT_BINARY_DIR}/deps/render"
        )
    endif()
    if(EXISTS "${GYO_UI_EDITOR_REPOSITORY_ROOT}/ui/CMakeLists.txt" AND
       NOT TARGET GYO::Ui)
        add_subdirectory(
            "${GYO_UI_EDITOR_REPOSITORY_ROOT}/ui"
            "${CMAKE_CURRENT_BINARY_DIR}/deps/ui"
        )
    endif()
    set(BUILD_TESTING "${_gyo_ui_editor_saved_build_testing}")

endif()

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
    target_sources(gyo_ui_editor PRIVATE
        src/AssetPreviewContext.cpp
        src/EditorApp.cpp
        src/ImGuiSdlRendererHost.cpp
        src/PreviewAdapter.cpp
    )
    target_link_libraries(gyo_ui_editor PRIVATE
        GYO::PlatformSDL
        GYO::RenderBackendSDL
        GYO::AssetSdlImage
        GYO::TextBackendSDLTTF
        imgui::core
        imgui::sdl3_sdlrenderer3
    )
    target_compile_definitions(gyo_ui_editor PRIVATE
        GYO_UI_EDITOR_WITH_GUI=1
    )

    gyo_copy_runtime_dlls(gyo_ui_editor)
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
else()
    target_compile_options(gyo_ui_editor_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(gyo_ui_editor PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(gyo_ui_editor_core_tests tests/EditorCoreTests.cpp)
    target_compile_features(gyo_ui_editor_core_tests PRIVATE cxx_std_20)
    target_link_libraries(gyo_ui_editor_core_tests PRIVATE gyo_ui_editor_core)
    add_test(NAME gyo_ui_editor.core COMMAND gyo_ui_editor_core_tests)
endif()
