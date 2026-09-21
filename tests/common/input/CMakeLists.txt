add_executable(input_tests
    InputActionMapTests.cpp
)

target_link_libraries(input_tests PRIVATE
    GYO::Input
    doctest::doctest
)

target_compile_features(input_tests PRIVATE cxx_std_20)

add_test(NAME input_tests COMMAND input_tests)

if(TARGET GYO::InputBackendSDL)
    add_executable(input_sdl_tests SdlInputTests.cpp)
    target_link_libraries(input_sdl_tests PRIVATE
        GYO::InputBackendSDL
        doctest::doctest
    )
    gyo_copy_runtime_dlls(input_sdl_tests)
    add_test(NAME input_sdl_tests COMMAND input_sdl_tests)
    set_tests_properties(input_sdl_tests PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy"
        TIMEOUT 15
    )
endif()
