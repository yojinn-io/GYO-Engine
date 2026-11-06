add_executable(gyo_voxel_mesh_tests VoxelMeshTests.cpp VoxelMapTests.cpp)
target_link_libraries(gyo_voxel_mesh_tests PRIVATE GYO::VoxelMesh doctest::doctest)
add_test(NAME gyo_voxel_mesh_tests COMMAND gyo_voxel_mesh_tests)

# Design Invariant 1, names: the module, its tests and its contract name no
# gameplay concept (docs/architecture/voxel-mesh.md). The shared guard script
# scans only the paths this module names.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME gyo_voxel_mesh.neutral_names
    COMMAND "${Python3_EXECUTABLE}" "${GYO_REPOSITORY_ROOT}/tests/common/scripts/neutral_names_guard.py"
        --root "${GYO_REPOSITORY_ROOT}"
        --scan engine/voxel_mesh tests/common/voxel_mesh docs/architecture/voxel-mesh.md)
set_tests_properties(gyo_voxel_mesh.neutral_names PROPERTIES LABELS cpu TIMEOUT 60)

# VoxelMesh stays below render, collision and products: its links are exactly
# Base, Math and, privately, the JSON parser (docs/architecture/voxel-mesh.md,
# Design Invariant 1; gyo_expect_links is defined with the time tests).
# Checked once the whole project is configured.
function(gyo_expect_voxel_mesh_links)
    gyo_expect_links(gyo_voxel_mesh GYO::Base GYO::Math nlohmann_json::nlohmann_json)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_voxel_mesh_links)
