add_executable(gyo_voxel_map_editor_core_tests VoxelMapCoreTests.cpp VoxelMapEditingTests.cpp)
target_compile_features(gyo_voxel_map_editor_core_tests PRIVATE cxx_std_20)
target_link_libraries(gyo_voxel_map_editor_core_tests PRIVATE gyo_voxel_map_editor_core doctest::doctest)
target_compile_definitions(gyo_voxel_map_editor_core_tests PRIVATE
    GYO_VOXEL_MAP_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
if(MSVC)
    target_compile_options(gyo_voxel_map_editor_core_tests PRIVATE /utf-8)
    target_compile_definitions(gyo_voxel_map_editor_core_tests PRIVATE NOMINMAX)
endif()
add_test(NAME gyo_voxel_map_editor.core COMMAND gyo_voxel_map_editor_core_tests)
set_tests_properties(gyo_voxel_map_editor.core PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

# The chunked map (v8-arch-big-map-tool-chunked, judgments T1 to T4).
add_executable(gyo_voxel_map_editor_chunked_tests VoxelMapChunkedTests.cpp)
target_compile_features(gyo_voxel_map_editor_chunked_tests PRIVATE cxx_std_20)
target_link_libraries(gyo_voxel_map_editor_chunked_tests PRIVATE gyo_voxel_map_editor_core doctest::doctest)
target_compile_definitions(gyo_voxel_map_editor_chunked_tests PRIVATE
    GYO_VOXEL_MAP_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    GYO_VOXEL_MAP_WORK="${CMAKE_CURRENT_BINARY_DIR}/chunked-work")
if(MSVC)
    target_compile_options(gyo_voxel_map_editor_chunked_tests PRIVATE /utf-8)
    target_compile_definitions(gyo_voxel_map_editor_chunked_tests PRIVATE NOMINMAX)
endif()
add_test(NAME gyo_voxel_map_editor.chunked COMMAND gyo_voxel_map_editor_chunked_tests)
set_tests_properties(gyo_voxel_map_editor.chunked PROPERTIES LABELS "cpu;editor" TIMEOUT 120)

add_test(NAME gyo_voxel_map_editor.cli
    COMMAND ${CMAKE_COMMAND} "-DEDITOR=$<TARGET_FILE:gyo_voxel_map_editor>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-work" "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        "-DHAS_WINDOW=$<BOOL:$<TARGET_EXISTS:gyo_voxel_map_editor_window>>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/VoxelMapCliTests.cmake")
set_tests_properties(gyo_voxel_map_editor.cli PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

# voxel_map_editor Design Invariant 1 (docs/voxel_map_editor/architecture.md): no game, no SDL, no ImGui.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME gyo_voxel_map_editor.dependency_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/voxel_map_editor_dependency_guard.py"
        --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME gyo_voxel_map_editor.dependency_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}"
        -p "test_voxel_map_editor_dependency_guard.py" -v)
set_tests_properties(gyo_voxel_map_editor.dependency_guard gyo_voxel_map_editor.dependency_guard_selftest
    PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

# W4: picking agrees with Engine's drawing, on a GPU (the offscreen device).
# Your Mac (Metal); CI's GPU step runs Engine's render.* tests only.
if(TARGET gyo_voxel_map_editor_window)
    add_executable(gyo_voxel_map_editor_pick_smoke VoxelMapPickSmoke.cpp)
    target_compile_features(gyo_voxel_map_editor_pick_smoke PRIVATE cxx_std_20)
    target_include_directories(gyo_voxel_map_editor_pick_smoke PRIVATE "${GYO_REPOSITORY_ROOT}/tools/voxel_map_editor/src")
    target_link_libraries(gyo_voxel_map_editor_pick_smoke PRIVATE gyo_voxel_map_editor_window GYO::ToolFrameworkPreview)
    target_compile_definitions(gyo_voxel_map_editor_pick_smoke PRIVATE
        GYO_TEST_SHADER_BUNDLE="${PROJECT_BINARY_DIR}/shaders/builtin")
    add_dependencies(gyo_voxel_map_editor_pick_smoke gyo_builtin_shaders)
    gyo_copy_runtime_dlls(gyo_voxel_map_editor_pick_smoke)
    add_test(NAME gyo_voxel_map_editor.pick COMMAND gyo_voxel_map_editor_pick_smoke)
    set_tests_properties(gyo_voxel_map_editor.pick PROPERTIES LABELS "gpu;editor" TIMEOUT 60)
endif()
