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
endif()
