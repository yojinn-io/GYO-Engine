# The ImGui binding (docs/framework/architecture.md, Design Invariants 2 and 6):
# ImGui on Engine's SDL renderer, run by the windowed shell. Built where the
# build has the windowed shell, the SDL renderer, Engine's SDL input (typed text
# and the input method's composition) and ImGui's SDL backends.
if(NOT TARGET GYO::ToolFrameworkWindow OR NOT TARGET GYO::RenderBackendSDLNative OR
   NOT TARGET GYO::InputBackendSDL OR NOT TARGET imgui::sdl3_sdlrenderer3)
    return()
endif()

add_library(gyo_tool_framework_imgui STATIC src/ImGuiShell.cpp)
add_library(GYO::ToolFrameworkImGui ALIAS gyo_tool_framework_imgui)
target_include_directories(gyo_tool_framework_imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# The public header names the windowed shell, the renderer it lends and
# Engine's input frame (the core links Engine's input publicly); ImGui, its
# SDL backends and the native layer stay private.
target_link_libraries(gyo_tool_framework_imgui
    PUBLIC
        GYO::ToolFrameworkWindow
        GYO::RenderBackendSDL
    PRIVATE
        GYO::Os
        # Typed text and the input method's composition (Design Invariant 2).
        GYO::InputBackendSDL
        GYO::PlatformSDLNative
        GYO::RenderBackendSDLNative
        imgui::core
        imgui::sdl3_sdlrenderer3
)
gyo_framework_warnings(gyo_tool_framework_imgui)
