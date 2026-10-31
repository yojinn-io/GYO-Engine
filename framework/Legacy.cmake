# The framework core's legacy links and targets (docs/framework/architecture.md, Design Invariant 2); see
# framework/CMakeLists.txt. Removed in v8-framework-invariants.

# The catalog registration parses JSON in the core until the JSON binding replaces it.
target_link_libraries(gyo_tool_framework PRIVATE nlohmann_json::nlohmann_json)

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
