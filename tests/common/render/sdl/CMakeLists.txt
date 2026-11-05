# The SDL 2D renderer's streaming textures (v8-engine-sdl-renderer-texture, T1),
# on SDL's dummy video driver and software renderer.
if(TARGET GYO::RenderBackendSDLNative)
    add_executable(gyo_sdl_renderer_texture_tests SdlRendererTextureTests.cpp)
    target_link_libraries(gyo_sdl_renderer_texture_tests PRIVATE GYO::RenderBackendSDLNative GYO::PlatformSDLNative
                                                                 doctest::doctest)
    target_compile_features(gyo_sdl_renderer_texture_tests PRIVATE cxx_std_20)
    gyo_copy_runtime_dlls(gyo_sdl_renderer_texture_tests)
    add_test(NAME render.sdl_renderer_texture COMMAND gyo_sdl_renderer_texture_tests)
    set_tests_properties(render.sdl_renderer_texture PROPERTIES
        LABELS "cpu" ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30)
endif()
