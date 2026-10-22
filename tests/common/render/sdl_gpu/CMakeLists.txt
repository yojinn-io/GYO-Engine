if(TARGET GYO::RenderBackendSDLGPU)
    gyo_add_shader_bundle(gyo_test_custom_shaders
        SPEC "${CMAKE_CURRENT_SOURCE_DIR}/../../shader_pipeline/fixtures/bundle.json"
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders/custom")
    add_executable(gyo_sdl_gpu_mesh_smoke MeshUpdateSmoke.cpp RenderFeatureSmoke.cpp)
    target_link_libraries(gyo_sdl_gpu_mesh_smoke PRIVATE GYO::RenderBackendSDLGPU)
    target_compile_definitions(gyo_sdl_gpu_mesh_smoke PRIVATE NOMINMAX
        GYO_TEST_SHADER_BUNDLE="${PROJECT_BINARY_DIR}/shaders/builtin"
        GYO_TEST_CUSTOM_SHADER_BUNDLE="${gyo_test_custom_shaders_DIRECTORY}")
    add_dependencies(gyo_sdl_gpu_mesh_smoke gyo_builtin_shaders gyo_test_custom_shaders)
    gyo_copy_runtime_dlls(gyo_sdl_gpu_mesh_smoke)
    add_test(NAME render.sdl_gpu_mesh_smoke COMMAND gyo_sdl_gpu_mesh_smoke)
    set_tests_properties(render.sdl_gpu_mesh_smoke PROPERTIES TIMEOUT 30 LABELS "gpu")

    # The capture file writer needs no GPU (IP-4 ip4b).
    add_executable(gyo_sdl_gpu_capture_tests SceneCaptureFileTests.cpp)
    target_link_libraries(gyo_sdl_gpu_capture_tests PRIVATE GYO::RenderBackendSDLGPU GYO::PlatformSDLNative doctest::doctest)
    target_compile_features(gyo_sdl_gpu_capture_tests PRIVATE cxx_std_20)
    gyo_copy_runtime_dlls(gyo_sdl_gpu_capture_tests)
    add_test(NAME render.sdl_gpu_capture_file COMMAND gyo_sdl_gpu_capture_tests)
    set_tests_properties(render.sdl_gpu_capture_file PROPERTIES TIMEOUT 15 LABELS "cpu")
endif()
