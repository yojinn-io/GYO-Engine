# kernel.el-silent (error levels D1, docs/architecture/target/v2/error-levels-declaration.zh-Hant.md
# section 9.2): the kernel writes nothing to standard error on its Err paths. It has no error
# handler; the outermost Run or Trigger returns the Err once and whoever handles it logs it.
# kernel_guard K6 scans the kernel's text; this runs the paths and reads what a process wrote.
#   cmake -DTESTS=<gyo_kernel_tests> -DPOSITIVE=<gyo_kernel_silent_probe_positive> -P KernelSilentProbe.cmake
# 1. Positive control: the capture sees a line a process writes to stderr.
# 2. Each kernel test case below takes an Err path (a System's Err, a hook's Err, a throw, a
#    nested Err, a hook round limit in a Run and in the Host's Trigger, an exception out of an
#    immediate change). It must run exactly one test case, pass, and leave stderr empty
#    (doctest reports on standard output). Before error levels D1 the Executor's default
#    handler wrote "kernel error:" to stderr on each of these paths.
# Filters are name prefixes with no comma: doctest splits --test-case on commas.
if(NOT TESTS OR NOT POSITIVE)
    message(FATAL_ERROR "TESTS and POSITIVE are required")
endif()

execute_process(COMMAND "${POSITIVE}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(FIND "${err}" "GYO silent probe: positive control" found)
if(NOT result STREQUAL "0" OR found EQUAL -1)
    message(FATAL_ERROR "el-silent: the positive control was not captured (result ${result}); stderr:\n${err}")
endif()

set(cases
    "J-35: a hook returns Err*"
    "J-35: a throw*"
    "J-56: an inner System's Err is reported once*"
    "J-54: at the limit*"
    "J-59: the Host's Trigger stops at KernelSettings::hookRoundLimit*"
    "DI-KS 18: an exception that leaves an immediate change interrupts the World*")
foreach(filter IN LISTS cases)
    execute_process(COMMAND "${TESTS}" "--test-case=${filter}" "--no-version"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "el-silent: [${filter}] failed (result ${result}):\n${out}\n${err}")
    endif()
    if(NOT out MATCHES "test cases: +1 \\| +1 passed")
        message(FATAL_ERROR "el-silent: [${filter}] did not run exactly one test case:\n${out}")
    endif()
    string(STRIP "${err}" stripped)
    if(NOT stripped STREQUAL "")
        message(FATAL_ERROR "el-silent: [${filter}] wrote to stderr (the kernel must write nothing):\n${err}")
    endif()
endforeach()
message(STATUS "el-silent: positive control captured; ${filter} and the other cases left stderr empty")
