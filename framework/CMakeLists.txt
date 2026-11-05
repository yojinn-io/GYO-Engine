# The layer games and tools share (docs/framework/architecture.md). It is not a
# product: GyoBuild adds it once, after the engine, and only while something
# selected uses it: a tool, or a game that declares the component FRAMEWORK
# (Design Invariant 1).

# The warnings every framework target builds with, the bindings' included.
function(gyo_framework_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(${target} PRIVATE NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(gyo_tool_framework STATIC
    src/AbsolutePath.cpp
    src/CatalogPlan.cpp
    src/EventSounds.cpp
    src/HeadlessShell.cpp
    src/ReadOnlyAssetCatalog.cpp
    src/UiTextInput.cpp
)
add_library(GYO::ToolFramework ALIAS gyo_tool_framework)
target_include_directories(gyo_tool_framework PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(gyo_tool_framework PUBLIC cxx_std_20)
# The catalog mount names engine/asset types; paths go to UTF-8 and the
# command line comes as UTF-8 through GYO::Os. The text input mapping turns
# Engine's input entries into Ui's (v8-input-ui-adapter). Event sounds bind to
# Engine's event set (GYO::Model, in the public header) and check gains with
# GYO::Audio (docs/framework/event-sounds.md).
target_link_libraries(gyo_tool_framework
    PUBLIC GYO::Engine GYO::Input GYO::Ui GYO::Model
    PRIVATE GYO::Os GYO::Audio
)
set(_gyo_framework_targets gyo_tool_framework)

# The windowed shell, where the build has Engine's SDL platform. It reaches SDL
# only through that neutral target and creates no render device.
if(TARGET GYO::PlatformSDL)
    add_library(gyo_tool_framework_window STATIC src/WindowShell.cpp)
    add_library(GYO::ToolFrameworkWindow ALIAS gyo_tool_framework_window)
    target_link_libraries(gyo_tool_framework_window PUBLIC gyo_tool_framework GYO::PlatformSDL)
    list(APPEND _gyo_framework_targets gyo_tool_framework_window)
endif()

# The offscreen 3D preview (docs/framework/preview.md), where the build has
# Engine's SDL GPU backend and its SDL renderer: the device, Engine's renderer
# and the readback put together, the frame uploaded as a texture of the
# window's renderer. The client creates and holds it; no shell does.
if(TARGET GYO::RenderBackendSDLGPU AND TARGET GYO::RenderBackendSDL)
    add_library(gyo_tool_framework_preview STATIC src/Preview.cpp src/PreviewCamera.cpp src/PreviewTexture.cpp)
    add_library(GYO::ToolFrameworkPreview ALIAS gyo_tool_framework_preview)
    target_link_libraries(gyo_tool_framework_preview
        PUBLIC gyo_tool_framework GYO::PlatformSDL GYO::Render GYO::RenderBackendSDLGPU GYO::RenderBackendSDL
        PRIVATE GYO::Os
    )
    list(APPEND _gyo_framework_targets gyo_tool_framework_preview)
endif()

foreach(_target IN LISTS _gyo_framework_targets)
    gyo_framework_warnings(${_target})
endforeach()
unset(_gyo_framework_targets)

# Every binding directory that has a CMakeLists.txt; the core names none of
# them (Design Invariant 2). Each decides from the targets it needs whether it
# builds; tests/framework checks that the expected ones exist.
file(GLOB _gyo_bindings LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/bindings/*")
list(SORT _gyo_bindings)
foreach(_binding IN LISTS _gyo_bindings)
    if(EXISTS "${_binding}/CMakeLists.txt")
        add_subdirectory("${_binding}")
    endif()
endforeach()
unset(_gyo_bindings)
