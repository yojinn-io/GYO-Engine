# ui_render (docs/architecture/target/v2/ui.zh-Hant.md, section 4.7; Bevy's bevy_ui_render): extracts the UI draw lists
# and the glyph atlas's changes and queues one 2D pass of UI per camera.
add_library(gyo_ui_render STATIC
    src/UiRenderModule.cpp
)
add_library(GYO::UiRender ALIAS gyo_ui_render)

target_include_directories(gyo_ui_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
target_link_libraries(gyo_ui_render PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(ui_render)
target_compile_features(gyo_ui_render PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_ui_render PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(gyo_ui_render PRIVATE -Wall -Wextra -Wpedantic)
endif()
