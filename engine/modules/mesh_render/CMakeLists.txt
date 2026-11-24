# mesh_render (docs/architecture/target/v2/render.zh-Hant.md, section 8; Bevy's bevy_pbr meshes): extracts Mesh3ds,
# keeps their model meshes as retained meshes, skins on the CPU and queues one 3D pass per 3D camera.
add_library(gyo_mesh_render STATIC
    src/MeshRenderModule.cpp
)
add_library(GYO::MeshRender ALIAS gyo_mesh_render)

target_include_directories(gyo_mesh_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
target_link_libraries(gyo_mesh_render PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(mesh_render)
target_compile_features(gyo_mesh_render PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_mesh_render PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_mesh_render PRIVATE -Wall -Wextra -Wpedantic)
endif()
