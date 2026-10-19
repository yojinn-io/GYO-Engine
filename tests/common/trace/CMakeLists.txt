add_executable(gyo_trace_tests TraceSinkTests.cpp)
target_link_libraries(gyo_trace_tests PRIVATE GYO::Trace gyo_test_support doctest::doctest)
add_test(NAME gyo_trace_tests COMMAND gyo_trace_tests)
set_tests_properties(gyo_trace_tests PROPERTIES LABELS "cpu" TIMEOUT 120)

# The trace transport is a leaf above the role threads: its links, private and
# interface alike, are exactly GYO::Threads (gyo_expect_links is defined with
# the time tests, which configure first). Checked once the whole project is
# configured.
function(gyo_expect_trace_links)
    gyo_expect_links(gyo_trace GYO::Threads)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_trace_links)
