# world_ui (docs/architecture/target/v2/ui.zh-Hant.md, section 4.6): the World end of ui. It registers the roots, the
# actions, the frame's UI input and UiText, maps input to UiInput, runs each root's runtime and composes its draw list.
add_library(gyo_world_ui STATIC
    src/WorldUiModule.cpp
)
add_library(GYO::WorldUi ALIAS gyo_world_ui)

target_include_directories(gyo_world_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
target_link_libraries(gyo_world_ui PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(world_ui)
target_compile_features(gyo_world_ui PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_world_ui PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(gyo_world_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()
