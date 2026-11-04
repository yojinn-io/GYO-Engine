add_executable(gyo_model_viewer_core_tests ModelViewerCoreTests.cpp ModelViewerPlaybackTests.cpp
                                           ModelViewerEventEditingTests.cpp ModelViewerEventSoundsTests.cpp)
target_compile_features(gyo_model_viewer_core_tests PRIVATE cxx_std_20)
target_link_libraries(gyo_model_viewer_core_tests PRIVATE gyo_model_viewer_core GYO::Model GYO::ToolFrameworkJson
                                                          doctest::doctest)
target_compile_definitions(gyo_model_viewer_core_tests PRIVATE
    GYO_MODEL_VIEWER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME gyo_model_viewer.core COMMAND gyo_model_viewer_core_tests)
set_tests_properties(gyo_model_viewer.core PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

add_test(NAME gyo_model_viewer.cli
    COMMAND ${CMAKE_COMMAND} "-DVIEWER=$<TARGET_FILE:gyo_model_viewer>"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-work" "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ModelViewerCliTests.cmake")
set_tests_properties(gyo_model_viewer.cli PROPERTIES LABELS "cpu;editor" TIMEOUT 60)

# -preview V2 and V3 on a GPU (the offscreen device beside the SDL renderer):
# your Mac (Metal); CI's GPU step runs Engine's render.* tests only.
if(TARGET gyo_model_viewer_preview)
    add_executable(gyo_model_viewer_preview_smoke ModelViewerPreviewSmoke.cpp)
    target_compile_features(gyo_model_viewer_preview_smoke PRIVATE cxx_std_20)
    target_link_libraries(gyo_model_viewer_preview_smoke PRIVATE gyo_model_viewer_preview GYO::RenderBackendSDLGPU
                                                                 GYO::RenderBackendSDLNative)
    target_compile_definitions(gyo_model_viewer_preview_smoke PRIVATE
        GYO_TEST_SHADER_BUNDLE="${PROJECT_BINARY_DIR}/shaders/builtin")
    add_dependencies(gyo_model_viewer_preview_smoke gyo_builtin_shaders)
    gyo_copy_runtime_dlls(gyo_model_viewer_preview_smoke)
    add_test(NAME gyo_model_viewer.preview COMMAND gyo_model_viewer_preview_smoke)
    set_tests_properties(gyo_model_viewer.preview PROPERTIES LABELS "gpu;editor" TIMEOUT 60)
endif()

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
