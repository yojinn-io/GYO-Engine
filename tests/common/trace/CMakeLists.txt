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

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariant TR-1 (link side): GYO::Base publicly; GYO::Logging and GYO::Time privately
#.
function(gyo_expect_trace_links)
    gyo_expect_links(gyo_trace GYO::Base GYO::Logging GYO::Time)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_trace_links)

# Judgement 7 (measured only): run by hand, Release; not a ctest.
add_executable(gyo_trace_budget TraceBudget.cpp)
target_link_libraries(gyo_trace_budget PRIVATE GYO::Trace GYO::Logging GYO::Orchestration GYO::Time)
if(MSVC)
    target_compile_options(gyo_trace_budget PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_trace_budget PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_trace_budget PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Design Invariant TR-1: the trace Module's include scan, set up after scenario segment 4 passed (AGENTS §3 on
# when to set up guards that bind the way of implementing).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME trace.guard
    COMMAND Python3::Interpreter -I -B "${CMAKE_CURRENT_SOURCE_DIR}/trace_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME trace.guard_selftest
    COMMAND Python3::Interpreter -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_trace_guard.py" -v)
set_tests_properties(trace.guard trace.guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)
