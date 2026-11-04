add_executable(gyo_model_viewer_core_tests ModelViewerCoreTests.cpp)
target_compile_features(gyo_model_viewer_core_tests PRIVATE cxx_std_20)
target_link_libraries(gyo_model_viewer_core_tests PRIVATE gyo_model_viewer_core GYO::Model doctest::doctest)
target_compile_definitions(gyo_model_viewer_core_tests PRIVATE
    GYO_MODEL_VIEWER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME gyo_model_viewer.core COMMAND gyo_model_viewer_core_tests)
set_tests_properties(gyo_model_viewer.core PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

add_test(NAME gyo_model_viewer.cli
    COMMAND ${CMAKE_COMMAND} "-DVIEWER=$<TARGET_FILE:gyo_model_viewer>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-work" "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ModelViewerCliTests.cmake")
set_tests_properties(gyo_model_viewer.cli PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

# model_viewer Design Invariant 1 (docs/model_viewer/architecture.md): no game, and no SDL of its own.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME gyo_model_viewer.dependency_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/model_viewer_dependency_guard.py"
        --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME gyo_model_viewer.dependency_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}"
        -p "test_model_viewer_dependency_guard.py" -v)
set_tests_properties(gyo_model_viewer.dependency_guard gyo_model_viewer.dependency_guard_selftest
    PROPERTIES LABELS "cpu;editor" TIMEOUT 60)
