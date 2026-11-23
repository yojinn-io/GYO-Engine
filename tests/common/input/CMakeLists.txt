# Tests of the input and window Modules and the window_camera glue (docs/architecture/target/v2/input.zh-Hant.md,
# window.zh-Hant.md). Synthetic writers only: no backend.
add_executable(gyo_input_tests TestMain.cpp InputTests.cpp)
target_link_libraries(gyo_input_tests PRIVATE GYO::Input GYO::Window GYO::WindowCamera GYO::Camera GYO::Transform
                      gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_input_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_input_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_input_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME input.module COMMAND gyo_input_tests "--test-case=input:*")
add_test(NAME window.module COMMAND gyo_input_tests "--test-case=window:*,window_camera:*")
set_tests_properties(input.module window.module PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# IN-1, WN-1: input and window link the orchestration layer, the kernel, Base, Math and Time, nothing else (no other
# Module, no Plugin); window_camera links window and camera only besides them.
function(gyo_expect_window_input_links)
    gyo_expect_links(gyo_input GYO::Base GYO::Kernel GYO::Math GYO::Orchestration GYO::Time)
    gyo_expect_links(gyo_window GYO::Base GYO::Kernel GYO::Math GYO::Orchestration GYO::Time)
    gyo_expect_links(gyo_window_camera GYO::Base GYO::Camera GYO::Kernel GYO::Orchestration GYO::Window)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_window_input_links)
