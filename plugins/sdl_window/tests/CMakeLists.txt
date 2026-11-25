# Tests of sdl_window (docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_window"). They come and go with its row
# (Design Invariant SDL-2). They push SDL events, so they are adapter tests (tests/adapters/sdl/, SDL-1) and run on
# SDL's dummy video driver: no display, every CI row.
add_executable(gyo_sdl_window_tests TestMain.cpp adapters/sdl/SDLWindowTests.cpp)
target_link_libraries(gyo_sdl_window_tests PRIVATE GYO::SDLWindow GYO::Input GYO::Window GYO::WindowCamera GYO::Camera GYO::WorldTime
                      GYO::Transform SDL3::SDL3-static doctest::doctest)
if(MSVC)
    target_compile_options(gyo_sdl_window_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_sdl_window_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_sdl_window_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME sdl_window.events COMMAND gyo_sdl_window_tests "--test-case=sdl_window:*")
set_tests_properties(sdl_window.events PROPERTIES LABELS "cpu" TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
