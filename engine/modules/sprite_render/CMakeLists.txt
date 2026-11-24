# sprite_render (docs/architecture/target/v2/render.zh-Hant.md, section 7; Bevy's bevy_sprite_render): extracts sprites
# and 2D shapes, makes textures of their images and queues one 2D pass per 2D camera.
add_library(gyo_sprite_render STATIC
    src/SpriteRenderModule.cpp
)
add_library(GYO::SpriteRender ALIAS gyo_sprite_render)

target_include_directories(gyo_sprite_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
target_link_libraries(gyo_sprite_render PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(sprite_render)
target_compile_features(gyo_sprite_render PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_sprite_render PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_sprite_render PRIVATE -Wall -Wextra -Wpedantic)
endif()
