# The Match's command line for match-ingress.jsonl (v7 batch 08c F2b-2); see
# CMakeLists.txt. Every case ends before the Match listens: usage errors exit
# 2; an unopenable detail file exits 1, and so does the 2-spawn fixture arena
# (the Match refuses it after opening its traces).
foreach(name MATCH ARENA OUTPUT)
    if(NOT DEFINED ${name})
        message(FATAL_ERROR "match_ingress_cli_check.cmake needs -D${name}")
    endif()
endforeach()
file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")

# Runs the Match with ARGN; checks its exit code and that stderr matches pattern.
function(expect_match label code pattern)
    execute_process(COMMAND "${MATCH}" ${ARGN} WORKING_DIRECTORY "${OUTPUT}" TIMEOUT 20
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT "${result}" STREQUAL "${code}")
        message(FATAL_ERROR "${label}: Match exited with ${result}, want ${code}\nstdout: ${out}\nstderr: ${err}")
    endif()
    if(NOT err MATCHES "${pattern}")
        message(FATAL_ERROR "${label}: stderr does not match '${pattern}'\nstderr: ${err}")
    endif()
endfunction()

expect_match("ingress trace named *commands.jsonl" 2 "must not end in commands\\.jsonl"
    --movement-trace "${OUTPUT}/run-commands.jsonl" --ingress-trace "${OUTPUT}/run-ingress-commands.jsonl")
expect_match("ingress trace without a movement trace" 2 "--ingress-trace needs --movement-trace"
    --ingress-trace "${OUTPUT}/detail.jsonl")
if(EXISTS "${OUTPUT}/run-commands.jsonl" OR EXISTS "${OUTPUT}/detail.jsonl")
    message(FATAL_ERROR "a refused command line created a trace file")
endif()

expect_match("unopenable ingress trace" 1 "Cannot open ingress trace"
    --arena "${ARENA}" --listen "not-an-endpoint" --movement-trace "${OUTPUT}/open-commands.jsonl"
    --ingress-trace "${OUTPUT}/missing/detail.jsonl")

# The default sits next to the movement trace; the Match then stops before it listens.
expect_match("derived ingress trace" 1 "."
    --arena "${ARENA}" --listen "not-an-endpoint" --movement-trace "${OUTPUT}/derived-commands.jsonl")
if(NOT EXISTS "${OUTPUT}/derived-ingress.jsonl")
    message(FATAL_ERROR "derived-commands.jsonl did not get derived-ingress.jsonl next to it")
endif()
file(GLOB written RELATIVE "${OUTPUT}" "${OUTPUT}/*")
list(SORT written)
set(expected "derived-commands.jsonl;derived-ingress.jsonl;open-commands.jsonl")
if(NOT written STREQUAL expected)
    message(FATAL_ERROR "trace files ${written}, want ${expected}")
endif()

# Without a movement trace the Match writes no detail file.
file(MAKE_DIRECTORY "${OUTPUT}/plain")
execute_process(COMMAND "${MATCH}" --arena "${ARENA}" --listen "not-an-endpoint"
    WORKING_DIRECTORY "${OUTPUT}/plain" TIMEOUT 20 RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
file(GLOB plain RELATIVE "${OUTPUT}/plain" "${OUTPUT}/plain/*")
if(plain)
    message(FATAL_ERROR "the Match without a movement trace wrote ${plain}")
endif()
