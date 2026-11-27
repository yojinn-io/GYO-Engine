# world_render (docs/architecture/target/v2/render.zh-Hant.md, section 7): the source World end of render. It registers
# the drawables, clear colours and render targets (RenderTargets) an author puts in the source World, for the render
# World to extract, and runs WriteRenderTargetSizes (the targets' sizes into camera's CameraTargetSizes; section 3).
add_library(gyo_world_render STATIC
    src/WorldRenderModule.cpp
)
add_library(GYO::WorldRender ALIAS gyo_world_render)

target_include_directories(gyo_world_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
target_link_libraries(gyo_world_render PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(world_render)
target_compile_features(gyo_world_render PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_world_render PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_world_render PRIVATE -Wall -Wextra -Wpedantic)
endif()
