# ctest sdl.unattended_assertion: runs the skipped case that forces an SDL assertion and expects the process to end
# (not 0: SDL aborts) with the unattended handler's text (adapters/sdl/UnattendedAssertions.cpp). ctest itself counts
# a process that ends on a signal as failed whatever its output, so this script judges it. -DEXE=<gyo_sdl_tests>.
execute_process(COMMAND "${EXE}" "--test-case=unattended:*" --no-skip
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE result TIMEOUT 50)
if("${result}" STREQUAL "0")
    message(FATAL_ERROR "[sdl-assert-not-ended] the forced SDL assertion did not end the process\n${out}${err}")
endif()
if(NOT "${out}${err}" MATCHES "SDL assertion failed \\(unattended, aborting\\): [^\n]*gyo-forced-assertion")
    message(FATAL_ERROR "[sdl-assert-no-handler-text] the process ended (${result}) without the unattended handler's text\n${out}${err}")
endif()
message(STATUS "[sdl-assert-forced-ended] the forced SDL assertion ended the process (${result}) through the unattended handler")
