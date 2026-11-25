# The voxel_map Module: the chunked map a tool bakes (gyo.voxel_map version 1, the index, and gyo.voxel_region
# version 1, the regions) read, validated and expanded into world-space geometry and boxes, and the asset loaders of
# both (docs/architecture/target/v2/voxel-map.zh-Hant.md). No renderer, no physics and no World: a game puts the
# geometry into its own meshes and colliders. Not part of any World (library_only in config/modules.csv).
add_library(gyo_voxel_map STATIC
    src/VoxelMap.cpp
    src/VoxelMapLoaders.cpp
)
add_library(GYO::VoxelMap ALIAS gyo_voxel_map)

target_include_directories(gyo_voxel_map PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(gyo_voxel_map PUBLIC cxx_std_20)
# Links to core are written here; links to other Modules come from config/modules.csv (O-6).
target_link_libraries(gyo_voxel_map PUBLIC GYO::Base GYO::Math)
gyo_link_registered_dependencies(voxel_map)
if(MSVC)
    target_compile_options(gyo_voxel_map PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_voxel_map PRIVATE -Wall -Wextra -Wpedantic)
endif()
