# Runs gyo_assert_abort_probe in one mode and checks that it aborted after
# reporting the failed condition. Usage:
#   cmake -DPROBE=<path> -DMODE=<default|returning|nested> -P AssertAbortProbe.cmake
if(NOT PROBE OR NOT MODE)
    message(FATAL_ERROR "PROBE and MODE are required")
endif()

execute_process(
    COMMAND "${PROBE}" "${MODE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(result STREQUAL "0")
    message(FATAL_ERROR "probe (${MODE}) exited normally; stderr:\n${error}")
endif()
if(error MATCHES "probe continued after a failed assertion")
    message(FATAL_ERROR "probe (${MODE}) continued after the assertion")
endif()

set(expected "probeValue == 2 && std::is_same_v<int, int>")
if(MODE STREQUAL "default")
    string(FIND "${error}" "GYO_ASSERT failed: ${expected}" found)
elseif(MODE STREQUAL "returning")
    string(FIND "${error}" "probe handler saw: ${expected}" found)
elseif(MODE STREQUAL "nested")
    string(FIND "${error}" "GYO_ASSERT failed: inner == 1" found)
else()
    message(FATAL_ERROR "unknown MODE ${MODE}")
endif()
if(found EQUAL -1)
    message(FATAL_ERROR "probe (${MODE}) stderr lacks the expected report:\n${error}")
endif()

message(STATUS "probe (${MODE}) aborted as expected (result: ${result})")
