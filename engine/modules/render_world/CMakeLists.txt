# render_world (docs/architecture/target/v2/render.zh-Hant.md, section 5): the core of a World that draws, Bevy's
# RenderApp. It registers render's data in the World, inserts the Submit and Cleanup stages and empties the frame's
# data after it was drawn. No SDL, no backend (SDL declaration, B').
add_library(gyo_render_world STATIC
    src/RenderWorldModule.cpp
)
add_library(GYO::RenderWorld ALIAS gyo_render_world)

target_include_directories(gyo_render_world PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; render comes from config/modules.csv (O-6).
target_link_libraries(gyo_render_world PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(render_world)
target_compile_features(gyo_render_world PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_render_world PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_render_world PRIVATE -Wall -Wextra -Wpedantic)
endif()
