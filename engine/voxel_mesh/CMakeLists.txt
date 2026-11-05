# gyo.voxel_mesh: the baked geometry of a grid-built map (docs/architecture/voxel-mesh.md).
# Its links are exactly Base, Math and, privately, the JSON parser (Design Invariant 1),
# so a Match links it without Render.
add_library(gyo_voxel_mesh STATIC src/VoxelMesh.cpp)
add_library(GYO::VoxelMesh ALIAS gyo_voxel_mesh)
target_include_directories(gyo_voxel_mesh PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gyo_voxel_mesh PUBLIC GYO::Base GYO::Math PRIVATE nlohmann_json::nlohmann_json)
target_compile_features(gyo_voxel_mesh PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_voxel_mesh PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_voxel_mesh PRIVATE -Wall -Wextra -Wpedantic)
endif()
