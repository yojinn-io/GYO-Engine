add_executable(gyo_voxel_mesh_tests VoxelMeshTests.cpp)
target_link_libraries(gyo_voxel_mesh_tests PRIVATE GYO::VoxelMesh doctest::doctest)
add_test(NAME gyo_voxel_mesh_tests COMMAND gyo_voxel_mesh_tests)

# VoxelMesh stays below render, collision and products: its links are exactly
# Base, Math and, privately, the JSON parser (docs/architecture/voxel-mesh.md,
# Design Invariant 1; gyo_expect_links is defined with the time tests).
# Checked once the whole project is configured.
function(gyo_expect_voxel_mesh_links)
    gyo_expect_links(gyo_voxel_mesh GYO::Base GYO::Math nlohmann_json::nlohmann_json)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_voxel_mesh_links)
