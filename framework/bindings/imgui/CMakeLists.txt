# The ImGui binding (docs/framework/architecture.md, Design Invariants 2 and 6):
# ImGui on Engine's SDL renderer, run by the windowed shell. Built where the
# build has the windowed shell, the SDL renderer and ImGui's SDL backends.
if(NOT TARGET GYO::ToolFrameworkWindow OR NOT TARGET GYO::RenderBackendSDLNative OR
   NOT TARGET imgui::sdl3_sdlrenderer3)
    return()
endif()

add_library(gyo_tool_framework_imgui STATIC src/ImGuiShell.cpp)
add_library(GYO::ToolFrameworkImGui ALIAS gyo_tool_framework_imgui)
target_include_directories(gyo_tool_framework_imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# The public header names the windowed shell and the renderer it lends; ImGui,
# its SDL backends and the native layer stay private.
target_link_libraries(gyo_tool_framework_imgui
    PUBLIC
        GYO::ToolFrameworkWindow
        GYO::RenderBackendSDL
    PRIVATE
        GYO::Os
        GYO::PlatformSDLNative
        GYO::RenderBackendSDLNative
        imgui::core
        imgui::sdl3_sdlrenderer3
)
gyo_framework_warnings(gyo_tool_framework_imgui)
