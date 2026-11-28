# Tests of the sdl Plugin (docs/architecture/target/v2/sdl.zh-Hant.md). A
# Plugin's tests live in its directory, so they come and go with its row
# (Design Invariant SDL-2). Only tests/adapters/sdl/ includes SDL3/ (SDL-1).
# Every test executable that starts SDL links this: a failed SDL assertion ends the run, no dialog
# (adapters/sdl/UnattendedAssertions.hpp). An OBJECT library, so its handler's installer is always linked in.
add_library(gyo_sdl_unattended_assertions OBJECT adapters/sdl/UnattendedAssertions.cpp)
target_include_directories(gyo_sdl_unattended_assertions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/adapters/sdl)
target_link_libraries(gyo_sdl_unattended_assertions PRIVATE SDL3::SDL3-static)
target_compile_features(gyo_sdl_unattended_assertions PRIVATE cxx_std_20)
add_executable(gyo_sdl_tests TestMain.cpp ClockPairTests.cpp adapters/sdl/SDLBuildTests.cpp)
target_link_libraries(gyo_sdl_tests PRIVATE GYO::SDL SDL3::SDL3-static gyo_sdl_unattended_assertions doctest::doctest)
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
# A forced SDL assertion ends the process through the unattended handler, with no dialog (the case is skipped in
# other runs; it sets SDL's hint to abort too, so a missing handler also ends without a dialog, and fails on the text).
add_test(NAME sdl.unattended_assertion
    COMMAND ${CMAKE_COMMAND} "-DEXE=$<TARGET_FILE:gyo_sdl_tests>" -P "${CMAKE_CURRENT_SOURCE_DIR}/ExpectUnattendedAbort.cmake")
set_tests_properties(sdl.unattended_assertion PROPERTIES LABELS "cpu" TIMEOUT 60)
