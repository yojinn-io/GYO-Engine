# The render Module (docs/architecture/target/v2/render.zh-Hant.md; Design Invariants RD-1 to RD-4): what a renderer
# is asked to draw, as data (RenderCommands, RenderTextures), the neutral arithmetic of a 2D pass and the shaders'
# HLSL sources and ABI. No SDL, no GPU object, no backend interface (SDL declaration, B'): a backend Plugin reads the
# data and compiles the sources. A library, in no World (library_only in config/modules.csv).
add_library(gyo_render STATIC
    src/Compose.cpp
    src/Pipeline2d.cpp
    src/Pipeline3d.cpp
    src/Render.cpp
    src/RenderTargets.cpp
)
add_library(GYO::Render ALIAS gyo_render)

target_include_directories(gyo_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
target_link_libraries(gyo_render PUBLIC GYO::Base GYO::Kernel GYO::Math)
gyo_link_registered_dependencies(render)
target_compile_features(gyo_render PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_render PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_render PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The shaders' sources, for a backend's shader tool: GYO_RENDER_SHADER_DIR names the directory.
set_property(TARGET gyo_render PROPERTY GYO_RENDER_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
