add_executable(gyo_trace_tests TraceSinkTests.cpp)
# The long-path case reads its file back and removes it through GYO::Os.
target_link_libraries(gyo_trace_tests PRIVATE GYO::Trace GYO::Os gyo_test_support doctest::doctest)
add_test(NAME gyo_trace_tests COMMAND gyo_trace_tests)
set_tests_properties(gyo_trace_tests PROPERTIES LABELS "cpu" TIMEOUT 120)

# The trace transport is a leaf above the role threads and the OS layer: its
# links, private and interface alike, are exactly GYO::Threads and GYO::Os
# (docs/architecture/trace.md, Design Invariant 7; gyo_expect_links is defined
# with the time tests, which configure first). Checked once the whole project
# is configured.
function(gyo_expect_trace_links)
    gyo_expect_links(gyo_trace GYO::Threads GYO::Os)
    # GYO::Os is private: what links GYO::Trace gets no engine/os headers from it.
    get_target_property(interface gyo_trace INTERFACE_LINK_LIBRARIES)
    if("GYO::Os" IN_LIST interface)
        message(FATAL_ERROR "gyo_trace exposes GYO::Os to its users; link it PRIVATE")
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_trace_links)
