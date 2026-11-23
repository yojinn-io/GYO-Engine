# The physics_3d Module (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, section 4; Design Invariants
# P3-1 to P3-6, section 8): shapes, the pure queries, BoxSet, Collider, the spatial query functions and the
# registration unit Physics3dModule.
add_library(gyo_physics_3d STATIC
    src/Shapes.cpp
    src/Queries.cpp
    src/Physics3dModule.cpp
)
add_library(GYO::Physics3d ALIAS gyo_physics_3d)

target_include_directories(gyo_physics_3d PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules (transform)
# come from config/modules.csv (O-6). P3-6: base, math, kernel, orchestration and transform only.
target_link_libraries(gyo_physics_3d PUBLIC GYO::Base GYO::Kernel GYO::Orchestration GYO::Math)
gyo_link_registered_dependencies(physics_3d)
target_compile_features(gyo_physics_3d PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_physics_3d PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_physics_3d PRIVATE -Wall -Wextra -Wpedantic)
endif()
