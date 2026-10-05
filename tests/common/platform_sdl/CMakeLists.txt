add_executable(platform_sdl_tests SdlPlatformTests.cpp)
target_link_libraries(platform_sdl_tests PRIVATE
    GYO::PlatformSDL
    doctest::doctest
)
target_compile_features(platform_sdl_tests PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(platform_sdl_tests)
add_test(NAME platform_sdl_tests COMMAND platform_sdl_tests)
set_tests_properties(platform_sdl_tests PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    TIMEOUT 15
)
