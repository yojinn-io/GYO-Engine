# The shared layer of the design tools (docs/framework/architecture.md). It is
# not a tool: GyoBuild adds it once, before the selected tools, and only when a
# tool is selected (Design Invariant 1).

add_library(gyo_tool_framework STATIC
    src/CatalogRegistration.cpp
    src/FileService.cpp
    src/PathDialog.cpp
    src/ReadOnlyAssetCatalog.cpp
)
add_library(GYO::ToolFramework ALIAS gyo_tool_framework)
target_include_directories(gyo_tool_framework PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_tool_framework PUBLIC cxx_std_20)
# The catalog mount names engine/asset types; paths go to UTF-8 through GYO::Os.
target_link_libraries(gyo_tool_framework
    PUBLIC GYO::Engine
    PRIVATE GYO::Os nlohmann_json::nlohmann_json
)
set(_gyo_framework_targets gyo_tool_framework)

# The window host, only where the build has the SDL renderer and ImGui's SDL
# backends (a tool that needs it requests them as components).
if(TARGET GYO::PlatformSDLNative AND TARGET GYO::RenderBackendSDLNative AND TARGET imgui::sdl3_sdlrenderer3)
    add_library(gyo_tool_framework_gui STATIC
        src/ImGuiSdlHost.cpp
        src/SdlPathDialogs.cpp
    )
    add_library(GYO::ToolFrameworkGui ALIAS gyo_tool_framework_gui)
    target_link_libraries(gyo_tool_framework_gui
        PUBLIC gyo_tool_framework
        PRIVATE
            GYO::Os
            SDL3::SDL3
            GYO::PlatformSDL
            GYO::PlatformSDLNative
            GYO::RenderBackendSDL
            GYO::RenderBackendSDLNative
            imgui::core
            imgui::sdl3_sdlrenderer3
    )
    list(APPEND _gyo_framework_targets gyo_tool_framework_gui)
endif()

foreach(_target IN LISTS _gyo_framework_targets)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${_target} PRIVATE NOMINMAX)
    else()
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
unset(_gyo_framework_targets)
