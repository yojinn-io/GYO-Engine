# The voxel map editor's headless part: gyo.voxel_grid, orientations and the box merge (grid), the chunked bake read
# back by the voxel_map Module and compared with master's golden map (map), the Goxel import (gox) and the command line.
set(_gyo_voxel_map_editor_fixtures "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(gyo_voxel_map_editor_tests GridTests.cpp MapTests.cpp GoxImportTests.cpp TestMain.cpp)
target_link_libraries(gyo_voxel_map_editor_tests PRIVATE gyo_voxel_map_editor_core GYO::VoxelMap doctest::doctest)
target_compile_definitions(gyo_voxel_map_editor_tests PRIVATE
    GYO_VOXEL_MAP_FIXTURES="${_gyo_voxel_map_editor_fixtures}"
    GYO_VOXEL_MAP_WORK="${CMAKE_CURRENT_BINARY_DIR}/work")
if(MSVC)
    target_compile_options(gyo_voxel_map_editor_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_voxel_map_editor_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_voxel_map_editor_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_voxel_map_editor_tests COMMAND gyo_voxel_map_editor_tests)
set_tests_properties(gyo_voxel_map_editor_tests PROPERTIES LABELS "cpu" TIMEOUT 300)

add_test(NAME gyo_voxel_map_editor.cli
    COMMAND ${CMAKE_COMMAND} "-DEDITOR=$<TARGET_FILE:gyo_voxel_map_editor>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-work" "-DFIXTURES=${_gyo_voxel_map_editor_fixtures}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CliTests.cmake")
set_tests_properties(gyo_voxel_map_editor.cli PROPERTIES LABELS "cpu" TIMEOUT 300)
