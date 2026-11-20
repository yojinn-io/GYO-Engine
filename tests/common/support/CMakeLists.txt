# Header-only helpers shared by engine tests. gyo_test_support carries the
# GYO_ASSERT helpers (AssertTestSupport.hpp) and the UTF-8 path text tests
# hand to Engine (PathText.hpp); product tests may link it too.
add_library(gyo_test_support INTERFACE)
target_include_directories(gyo_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gyo_test_support INTERFACE GYO::Base doctest::doctest)

# The stderr probe of error levels D1 and D2 (SilentProbe.cmake): a suite's error paths
# leave standard error empty. gyo_silent_probe_positive is its positive control.
add_executable(gyo_silent_probe_positive SilentProbePositive.cpp)
set(GYO_SILENT_PROBE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/SilentProbe.cmake" CACHE INTERNAL "")
# gyo_add_silent_probe(<test name> <doctest target> <cases file>)
function(gyo_add_silent_probe name target cases)
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DTESTS=$<TARGET_FILE:${target}>
            -DPOSITIVE=$<TARGET_FILE:gyo_silent_probe_positive>
            -DCASES=${cases}
            -DNAME=${name}
            -P ${GYO_SILENT_PROBE_SCRIPT})
    set_tests_properties(${name} PROPERTIES LABELS "cpu" TIMEOUT 120)
endfunction()
