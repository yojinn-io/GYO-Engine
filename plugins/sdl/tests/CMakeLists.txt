# Tests of the sdl Plugin (docs/architecture/target/v2/sdl.zh-Hant.md). A
# Plugin's tests live in its directory, so they come and go with its row
# (Design Invariant SDL-2). Only tests/adapters/sdl/ includes SDL3/ (SDL-1).
add_executable(gyo_sdl_tests TestMain.cpp ClockPairTests.cpp adapters/sdl/SDLBuildTests.cpp)
target_link_libraries(gyo_sdl_tests PRIVATE GYO::SDL SDL3::SDL3-static doctest::doctest)
if(MSVC)
    target_compile_options(gyo_sdl_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_sdl_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME sdl.build COMMAND gyo_sdl_tests "--test-case=build:*")
set_tests_properties(sdl.build PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
add_test(NAME sdl.clock COMMAND gyo_sdl_tests "--test-case=clock:*")
set_tests_properties(sdl.clock PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
