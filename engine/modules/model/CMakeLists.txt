# The model Module: a model as plain CPU data (the node hierarchy, materials as
# data, triangle meshes with their skins, animation clips), the pure functions
# of skeletal animation's first layer (a clip's pose at a time, blending,
# additive motion, the instants a time crosses,
# retargeting, model space) and the loader of the "model" type (FBX). It is the
# engine's only adapter for ufbx: its header and calls appear only under
# src/adapters/ufbx/ (Design Invariant MD-1 of
# docs/architecture/target/v2/model.zh-Hant.md). No SDL, no renderer and no
# GPU, so a Match loads skeletons and clips and samples them with no device.
# Not part of any World (library_only in config/modules.csv).
add_library(gyo_model STATIC
    src/Animation.cpp
    src/FbxLoader.cpp
    src/ModelData.cpp
    src/Retarget.cpp
    src/adapters/ufbx/UfbxImport.cpp
)
add_library(GYO::Model ALIAS gyo_model)

target_include_directories(gyo_model
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(gyo_model PUBLIC cxx_std_20)
# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6).
# ufbx is linked here, privately, so no user of GYO::Model sees it.
target_link_libraries(gyo_model PUBLIC GYO::Base GYO::Math)
gyo_link_registered_dependencies(model)
target_link_libraries(gyo_model PRIVATE gyo_ufbx)
if(MSVC)
    target_compile_options(gyo_model PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_model PRIVATE -Wall -Wextra -Wpedantic)
endif()
