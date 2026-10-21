add_executable(platform_sdl_tests SdlPlatformTests.cpp)
target_link_libraries(platform_sdl_tests PRIVATE
    GYO::PlatformSDLNative
    doctest::doctest
)
target_compile_features(platform_sdl_tests PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(platform_sdl_tests)
add_test(NAME platform_sdl_tests COMMAND platform_sdl_tests)
set_tests_properties(platform_sdl_tests PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    TIMEOUT 15
)

# G1 and G2 of docs/architecture/platform-sdl.md (Design Invariant 1).
add_executable(platform_sdl_header_tests HeaderHygieneTests.cpp)
target_link_libraries(platform_sdl_header_tests PRIVATE GYO::PlatformSDL doctest::doctest)
if(TARGET GYO::InputBackendSDL)
    target_link_libraries(platform_sdl_header_tests PRIVATE GYO::InputBackendSDL)
    target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_HYGIENE_SDL_INPUT)
endif()
if(TARGET GYO::RenderBackendSDL)
    target_link_libraries(platform_sdl_header_tests PRIVATE GYO::RenderBackendSDL)
    target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_HYGIENE_SDL_RENDERER)
endif()
if(TARGET GYO::RenderBackendSDLGPU)
    target_link_libraries(platform_sdl_header_tests PRIVATE GYO::RenderBackendSDLGPU)
    target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_HYGIENE_SDL_GPU)
endif()
target_compile_definitions(platform_sdl_header_tests PRIVATE GYO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_features(platform_sdl_header_tests PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(platform_sdl_header_tests)
add_test(NAME platform_sdl_header_tests COMMAND platform_sdl_header_tests)
set_tests_properties(platform_sdl_header_tests PROPERTIES LABELS "cpu" TIMEOUT 60)
