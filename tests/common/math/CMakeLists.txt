add_executable(gyo_math_tests
    MathTests.cpp
    MathCharacterizationTests.cpp
)
target_link_libraries(gyo_math_tests PRIVATE GYO::Math doctest::doctest)
# Same warning level as the collision module so header code that would warn in
# a consumer fails here first.
if(MSVC)
    target_compile_options(gyo_math_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_math_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_math_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_math_tests COMMAND gyo_math_tests)
