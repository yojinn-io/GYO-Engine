# The visibility Module (big-map declaration section 13): CheckVisibility in the source World, which writes each 3D
# camera's VisibleEntities from the entities' Aabb and VisibilityRange (render's Drawables.hpp, registered by
# world_render), after Bevy's check_visibility. Not carried by default (config/modules.csv).
add_library(gyo_visibility STATIC
    src/Visibility.cpp
    src/VisibilityModule.cpp
)
add_library(GYO::Visibility ALIAS gyo_visibility)

target_include_directories(gyo_visibility PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Links to core, ecs and orchestration are written here; links to other Modules (render, transform, camera) come from
# config/modules.csv (O-6).
target_link_libraries(gyo_visibility PUBLIC GYO::Base GYO::Kernel GYO::Orchestration GYO::Math)
gyo_link_registered_dependencies(visibility)
target_compile_features(gyo_visibility PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_visibility PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_visibility PRIVATE -Wall -Wextra -Wpedantic)
endif()
