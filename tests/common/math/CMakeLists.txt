add_executable(gyo_math_tests
    MathTests.cpp
    MathCharacterizationTests.cpp
    MathFrustumTests.cpp
)
target_link_libraries(gyo_math_tests PRIVATE GYO::Math gyo_test_support doctest::doctest)
# Same warning level as the collision module so header code that would warn in
# a consumer fails here first.
if(MSVC)
    target_compile_options(gyo_math_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_math_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_math_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_math_tests COMMAND gyo_math_tests)

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Math links Base only (v8-arch-gyo2-core-not-registered: core's links are written by hand and pinned here).
function(gyo_expect_math_links)
    gyo_expect_links(gyo_math GYO::Base)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_math_links)
