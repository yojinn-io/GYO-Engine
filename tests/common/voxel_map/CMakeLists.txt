# The voxel_map Module's tests (docs/architecture/target/v2/voxel-map.zh-Hant.md): every rule of gyo.voxel_map and
# gyo.voxel_region version 1, the two steps of a region against the one-step ValidateVoxelChunk, the expansion's bits,
# and both loaders through the asset store. Offline: no device, no World.
add_executable(gyo_voxel_map_tests VoxelMapTests.cpp)
target_link_libraries(gyo_voxel_map_tests PRIVATE GYO::VoxelMap GYO::Asset gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_voxel_map_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_voxel_map_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_voxel_map_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_voxel_map_tests COMMAND gyo_voxel_map_tests)
set_tests_properties(gyo_voxel_map_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariant VM-1: the Module's links are exactly Base, Math, the asset loader interface and JSON (privately,
# through config/modules.csv), so no renderer, physics or World appears.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_links gyo_voxel_map GYO::Base GYO::Math GYO::Asset GYO::Json)
