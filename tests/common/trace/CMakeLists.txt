# Tests of the trace Module (docs/architecture/target/v2/trace-declaration.zh-Hant.md, section 9).
add_executable(gyo_trace_tests
    TestMain.cpp
    TraceTests.cpp
)
target_link_libraries(gyo_trace_tests PRIVATE GYO::Trace GYO::Logging gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_trace_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_trace_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_trace_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME trace.module COMMAND gyo_trace_tests "--test-case=trace:*")
set_tests_properties(trace.module PROPERTIES LABELS "cpu" TIMEOUT 60 FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

# Design Invariant TR-1 (link side): GYO::Base publicly; GYO::Logging and GYO::Time privately
# (gyo_expect_links comes from tests/common/time).
function(gyo_expect_trace_links)
    gyo_expect_links(gyo_trace GYO::Base GYO::Logging GYO::Time)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_trace_links)
