# O-3, the exit flow (error levels): runs the standard main example (MainExample.cpp) and reads
# what the process did. [or-main-ok]: a good frame exits 0 and writes nothing to stderr.
# [or-main-failure]: a broken frame exits 1 and stderr holds exactly one Error line of the
# orchestration category naming the World, the schedule and the System's error.
#   cmake -DEXAMPLE=<gyo_orchestration_main_example> -P MainExampleProbe.cmake
# EXAMPLE may also follow the script as an argument (the mutation runner substitutes whole
# arguments only): cmake -P MainExampleProbe.cmake <example>
if(NOT EXAMPLE AND CMAKE_ARGC GREATER 1)
    math(EXPR _last "${CMAKE_ARGC} - 1")
    set(EXAMPLE "${CMAKE_ARGV${_last}}")
endif()
if(NOT EXAMPLE)
    message(FATAL_ERROR "EXAMPLE is required")
endif()

execute_process(COMMAND "${EXAMPLE}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(STRIP "${err}" stripped)
if(NOT result STREQUAL "0" OR NOT stripped STREQUAL "")
    message(FATAL_ERROR "[or-main-ok] a good frame: exit ${result} (want 0), stderr:\n${err}")
endif()

execute_process(COMMAND "${EXAMPLE}" bad RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result STREQUAL "1")
    message(FATAL_ERROR "[or-main-failure-exit] a broken frame: exit ${result} (want 1), stderr:\n${err}")
endif()
string(REGEX MATCHALL "\\[orchestration\\] error:" lines "${err}")
list(LENGTH lines count)
if(NOT count EQUAL 1)
    message(FATAL_ERROR "[or-main-failure-line] a broken frame: ${count} Error lines (want 1), stderr:\n${err}")
endif()
foreach(part IN ITEMS "the Product ended" "'Main'" "'Bad'" "the example System failed")
    string(FIND "${err}" "${part}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "[or-main-failure-text] the Error line lacks \"${part}\":\n${err}")
    endif()
endforeach()
message(STATUS "orchestration.O-3-main: exit 0 on a good frame; exit 1 and one Error line on a broken one")
