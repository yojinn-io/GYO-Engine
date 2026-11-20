# Error levels D1, D2 (docs/architecture/target/v2/error-levels-declaration.zh-Hant.md section 9.2,
# [el-silent-*]): the Engine writes nothing to standard error on its error paths. The kernel has no
# error handler and the orchestration layer logs nothing; the error goes back to the caller. The text guards scan the source; this runs the paths and reads what a
# process wrote.
#   cmake -DTESTS=<doctest executable> -DPOSITIVE=<gyo_silent_probe_positive> -DCASES=<file> -DNAME=<label>
#         -P SilentProbe.cmake
# 1. Positive control: the capture sees a line a process writes to stderr.
# 2. Each line of CASES is a doctest --test-case filter (a name prefix with no comma: doctest splits
#    on commas) whose case takes an error path. It must run exactly one test case, pass, and leave
#    stderr empty (doctest reports on standard output).
# TESTS and POSITIVE may also follow the script as two arguments (the mutation runner substitutes
# whole arguments only): cmake -DCASES=<file> -DNAME=<label> -P SilentProbe.cmake <tests> <positive>
if(NOT TESTS AND CMAKE_ARGC GREATER 2)
    math(EXPR _last "${CMAKE_ARGC} - 1")
    math(EXPR _before "${CMAKE_ARGC} - 2")
    set(TESTS "${CMAKE_ARGV${_before}}")
    set(POSITIVE "${CMAKE_ARGV${_last}}")
endif()
if(NOT TESTS OR NOT POSITIVE OR NOT CASES OR NOT NAME)
    message(FATAL_ERROR "TESTS, POSITIVE, CASES and NAME are required")
endif()

execute_process(COMMAND "${POSITIVE}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(FIND "${err}" "GYO silent probe: positive control" found)
if(NOT result STREQUAL "0" OR found EQUAL -1)
    message(FATAL_ERROR "${NAME}: the positive control was not captured (result ${result}); stderr:\n${err}")
endif()

file(STRINGS "${CASES}" filters)
list(LENGTH filters count)
if(count EQUAL 0)
    message(FATAL_ERROR "${NAME}: ${CASES} names no case")
endif()
foreach(filter IN LISTS filters)
    execute_process(COMMAND "${TESTS}" "--test-case=${filter}" "--no-version"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "${NAME}: [${filter}] failed (result ${result}):\n${out}\n${err}")
    endif()
    if(NOT out MATCHES "test cases: +1 \\| +1 passed")
        message(FATAL_ERROR "${NAME}: [${filter}] did not run exactly one test case:\n${out}")
    endif()
    string(STRIP "${err}" stripped)
    if(NOT stripped STREQUAL "")
        message(FATAL_ERROR "${NAME}: [${filter}] wrote to stderr (the Engine must write nothing):\n${err}")
    endif()
endforeach()
message(STATUS "${NAME}: positive control captured; ${count} cases left stderr empty")
