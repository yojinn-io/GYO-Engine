# The transform Module (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 4.1; Design
# Invariants TF-1 to TF-4 in docs/architecture/target/v2/transform.zh-Hant.md): Transform, GlobalTransform and the
# registration unit TransformModule that keeps GlobalTransform equal to Transform for entities without a parent.
add_library(gyo_transform STATIC
    src/TransformModule.cpp
)
add_library(GYO::Transform ALIAS gyo_transform)

target_include_directories(gyo_transform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6). It depends on no other Module (TF-4).
target_link_libraries(gyo_transform PUBLIC GYO::Base GYO::Kernel GYO::Orchestration GYO::Math)
gyo_link_registered_dependencies(transform)
target_compile_features(gyo_transform PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_transform PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_transform PRIVATE -Wall -Wextra -Wpedantic)
endif()
